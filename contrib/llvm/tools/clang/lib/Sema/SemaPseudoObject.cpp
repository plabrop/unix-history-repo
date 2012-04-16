//===--- SemaPseudoObject.cpp - Semantic Analysis for Pseudo-Objects ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for expressions involving
//  pseudo-object references.  Pseudo-objects are conceptual objects
//  whose storage is entirely abstract and all accesses to which are
//  translated through some sort of abstraction barrier.
//
//  For example, Objective-C objects can have "properties", either
//  declared or undeclared.  A property may be accessed by writing
//    expr.prop
//  where 'expr' is an r-value of Objective-C pointer type and 'prop'
//  is the name of the property.  If this expression is used in a context
//  needing an r-value, it is treated as if it were a message-send
//  of the associated 'getter' selector, typically:
//    [expr prop]
//  If it is used as the LHS of a simple assignment, it is treated
//  as a message-send of the associated 'setter' selector, typically:
//    [expr setProp: RHS]
//  If it is used as the LHS of a compound assignment, or the operand
//  of a unary increment or decrement, both are required;  for example,
//  'expr.prop *= 100' would be translated to:
//    [expr setProp: [expr prop] * 100]
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Initialization.h"
#include "clang/AST/ExprObjC.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang;
using namespace sema;

namespace {
  // Basically just a very focused copy of TreeTransform.
  template <class T> struct Rebuilder {
    Sema &S;
    Rebuilder(Sema &S) : S(S) {}

    T &getDerived() { return static_cast<T&>(*this); }

    Expr *rebuild(Expr *e) {
      // Fast path: nothing to look through.
      if (typename T::specific_type *specific
            = dyn_cast<typename T::specific_type>(e))
        return getDerived().rebuildSpecific(specific);

      // Otherwise, we should look through and rebuild anything that
      // IgnoreParens would.

      if (ParenExpr *parens = dyn_cast<ParenExpr>(e)) {
        e = rebuild(parens->getSubExpr());
        return new (S.Context) ParenExpr(parens->getLParen(),
                                         parens->getRParen(),
                                         e);
      }

      if (UnaryOperator *uop = dyn_cast<UnaryOperator>(e)) {
        assert(uop->getOpcode() == UO_Extension);
        e = rebuild(uop->getSubExpr());
        return new (S.Context) UnaryOperator(e, uop->getOpcode(),
                                             uop->getType(),
                                             uop->getValueKind(),
                                             uop->getObjectKind(),
                                             uop->getOperatorLoc());
      }

      if (GenericSelectionExpr *gse = dyn_cast<GenericSelectionExpr>(e)) {
        assert(!gse->isResultDependent());
        unsigned resultIndex = gse->getResultIndex();
        unsigned numAssocs = gse->getNumAssocs();

        SmallVector<Expr*, 8> assocs(numAssocs);
        SmallVector<TypeSourceInfo*, 8> assocTypes(numAssocs);

        for (unsigned i = 0; i != numAssocs; ++i) {
          Expr *assoc = gse->getAssocExpr(i);
          if (i == resultIndex) assoc = rebuild(assoc);
          assocs[i] = assoc;
          assocTypes[i] = gse->getAssocTypeSourceInfo(i);
        }

        return new (S.Context) GenericSelectionExpr(S.Context,
                                                    gse->getGenericLoc(),
                                                    gse->getControllingExpr(),
                                                    assocTypes.data(),
                                                    assocs.data(),
                                                    numAssocs,
                                                    gse->getDefaultLoc(),
                                                    gse->getRParenLoc(),
                                      gse->containsUnexpandedParameterPack(),
                                                    resultIndex);
      }

      llvm_unreachable("bad expression to rebuild!");
    }
  };

  struct ObjCPropertyRefRebuilder : Rebuilder<ObjCPropertyRefRebuilder> {
    Expr *NewBase;
    ObjCPropertyRefRebuilder(Sema &S, Expr *newBase)
      : Rebuilder<ObjCPropertyRefRebuilder>(S), NewBase(newBase) {}

    typedef ObjCPropertyRefExpr specific_type;
    Expr *rebuildSpecific(ObjCPropertyRefExpr *refExpr) {
      // Fortunately, the constraint that we're rebuilding something
      // with a base limits the number of cases here.
      assert(refExpr->getBase());

      if (refExpr->isExplicitProperty()) {
        return new (S.Context)
          ObjCPropertyRefExpr(refExpr->getExplicitProperty(),
                              refExpr->getType(), refExpr->getValueKind(),
                              refExpr->getObjectKind(), refExpr->getLocation(),
                              NewBase);
      }
      return new (S.Context)
        ObjCPropertyRefExpr(refExpr->getImplicitPropertyGetter(),
                            refExpr->getImplicitPropertySetter(),
                            refExpr->getType(), refExpr->getValueKind(),
                            refExpr->getObjectKind(),refExpr->getLocation(),
                            NewBase);
    }
  };

  struct ObjCSubscriptRefRebuilder : Rebuilder<ObjCSubscriptRefRebuilder> {
    Expr *NewBase;
    Expr *NewKeyExpr;
    ObjCSubscriptRefRebuilder(Sema &S, Expr *newBase, Expr *newKeyExpr)
    : Rebuilder<ObjCSubscriptRefRebuilder>(S), 
      NewBase(newBase), NewKeyExpr(newKeyExpr) {}
    
    typedef ObjCSubscriptRefExpr specific_type;
    Expr *rebuildSpecific(ObjCSubscriptRefExpr *refExpr) {
      assert(refExpr->getBaseExpr());
      assert(refExpr->getKeyExpr());
      
      return new (S.Context)
        ObjCSubscriptRefExpr(NewBase,
                             NewKeyExpr,
                             refExpr->getType(), refExpr->getValueKind(),
                             refExpr->getObjectKind(),refExpr->getAtIndexMethodDecl(),
                             refExpr->setAtIndexMethodDecl(),
                             refExpr->getRBracket());
    }
  };
  
  class PseudoOpBuilder {
  public:
    Sema &S;
    unsigned ResultIndex;
    SourceLocation GenericLoc;
    SmallVector<Expr *, 4> Semantics;

    PseudoOpBuilder(Sema &S, SourceLocation genericLoc)
      : S(S), ResultIndex(PseudoObjectExpr::NoResult),
        GenericLoc(genericLoc) {}

    virtual ~PseudoOpBuilder() {}

    /// Add a normal semantic expression.
    void addSemanticExpr(Expr *semantic) {
      Semantics.push_back(semantic);
    }

    /// Add the 'result' semantic expression.
    void addResultSemanticExpr(Expr *resultExpr) {
      assert(ResultIndex == PseudoObjectExpr::NoResult);
      ResultIndex = Semantics.size();
      Semantics.push_back(resultExpr);
    }

    ExprResult buildRValueOperation(Expr *op);
    ExprResult buildAssignmentOperation(Scope *Sc,
                                        SourceLocation opLoc,
                                        BinaryOperatorKind opcode,
                                        Expr *LHS, Expr *RHS);
    ExprResult buildIncDecOperation(Scope *Sc, SourceLocation opLoc,
                                    UnaryOperatorKind opcode,
                                    Expr *op);

    ExprResult complete(Expr *syntacticForm);

    OpaqueValueExpr *capture(Expr *op);
    OpaqueValueExpr *captureValueAsResult(Expr *op);

    void setResultToLastSemantic() {
      assert(ResultIndex == PseudoObjectExpr::NoResult);
      ResultIndex = Semantics.size() - 1;
    }

    /// Return true if assignments have a non-void result.
    virtual bool assignmentsHaveResult() { return true; }

    virtual Expr *rebuildAndCaptureObject(Expr *) = 0;
    virtual ExprResult buildGet() = 0;
    virtual ExprResult buildSet(Expr *, SourceLocation,
                                bool captureSetValueAsResult) = 0;
  };

  /// A PseudoOpBuilder for Objective-C @properties.
  class ObjCPropertyOpBuilder : public PseudoOpBuilder {
    ObjCPropertyRefExpr *RefExpr;
    ObjCPropertyRefExpr *SyntacticRefExpr;
    OpaqueValueExpr *InstanceReceiver;
    ObjCMethodDecl *Getter;

    ObjCMethodDecl *Setter;
    Selector SetterSelector;

  public:
    ObjCPropertyOpBuilder(Sema &S, ObjCPropertyRefExpr *refExpr) :
      PseudoOpBuilder(S, refExpr->getLocation()), RefExpr(refExpr),
      SyntacticRefExpr(0), InstanceReceiver(0), Getter(0), Setter(0) {
    }

    ExprResult buildRValueOperation(Expr *op);
    ExprResult buildAssignmentOperation(Scope *Sc,
                                        SourceLocation opLoc,
                                        BinaryOperatorKind opcode,
                                        Expr *LHS, Expr *RHS);
    ExprResult buildIncDecOperation(Scope *Sc, SourceLocation opLoc,
                                    UnaryOperatorKind opcode,
                                    Expr *op);

    bool tryBuildGetOfReference(Expr *op, ExprResult &result);
    bool findSetter();
    bool findGetter();

    Expr *rebuildAndCaptureObject(Expr *syntacticBase);
    ExprResult buildGet();
    ExprResult buildSet(Expr *op, SourceLocation, bool);
  };

 /// A PseudoOpBuilder for Objective-C array/dictionary indexing.
 class ObjCSubscriptOpBuilder : public PseudoOpBuilder {
   ObjCSubscriptRefExpr *RefExpr;
   OpaqueValueExpr *InstanceBase;
   OpaqueValueExpr *InstanceKey;
   ObjCMethodDecl *AtIndexGetter;
   Selector AtIndexGetterSelector;
  
   ObjCMethodDecl *AtIndexSetter;
   Selector AtIndexSetterSelector;
  
 public:
    ObjCSubscriptOpBuilder(Sema &S, ObjCSubscriptRefExpr *refExpr) :
      PseudoOpBuilder(S, refExpr->getSourceRange().getBegin()), 
      RefExpr(refExpr),
    InstanceBase(0), InstanceKey(0), 
    AtIndexGetter(0), AtIndexSetter(0) { }
  
   ExprResult buildRValueOperation(Expr *op);
   ExprResult buildAssignmentOperation(Scope *Sc,
                                       SourceLocation opLoc,
                                       BinaryOperatorKind opcode,
                                       Expr *LHS, Expr *RHS);
   Expr *rebuildAndCaptureObject(Expr *syntacticBase);
   
   bool findAtIndexGetter();
   bool findAtIndexSetter();
  
   ExprResult buildGet();
   ExprResult buildSet(Expr *op, SourceLocation, bool);
 };

}

/// Capture the given expression in an OpaqueValueExpr.
OpaqueValueExpr *PseudoOpBuilder::capture(Expr *e) {
  // Make a new OVE whose source is the given expression.
  OpaqueValueExpr *captured = 
    new (S.Context) OpaqueValueExpr(GenericLoc, e->getType(),
                                    e->getValueKind(), e->getObjectKind(),
                                    e);
  
  // Make sure we bind that in the semantics.
  addSemanticExpr(captured);
  return captured;
}

/// Capture the given expression as the result of this pseudo-object
/// operation.  This routine is safe against expressions which may
/// already be captured.
///
/// \param Returns the captured expression, which will be the
///   same as the input if the input was already captured
OpaqueValueExpr *PseudoOpBuilder::captureValueAsResult(Expr *e) {
  assert(ResultIndex == PseudoObjectExpr::NoResult);

  // If the expression hasn't already been captured, just capture it
  // and set the new semantic 
  if (!isa<OpaqueValueExpr>(e)) {
    OpaqueValueExpr *cap = capture(e);
    setResultToLastSemantic();
    return cap;
  }

  // Otherwise, it must already be one of our semantic expressions;
  // set ResultIndex to its index.
  unsigned index = 0;
  for (;; ++index) {
    assert(index < Semantics.size() &&
           "captured expression not found in semantics!");
    if (e == Semantics[index]) break;
  }
  ResultIndex = index;
  return cast<OpaqueValueExpr>(e);
}

/// The routine which creates the final PseudoObjectExpr.
ExprResult PseudoOpBuilder::complete(Expr *syntactic) {
  return PseudoObjectExpr::Create(S.Context, syntactic,
                                  Semantics, ResultIndex);
}

/// The main skeleton for building an r-value operation.
ExprResult PseudoOpBuilder::buildRValueOperation(Expr *op) {
  Expr *syntacticBase = rebuildAndCaptureObject(op);

  ExprResult getExpr = buildGet();
  if (getExpr.isInvalid()) return ExprError();
  addResultSemanticExpr(getExpr.take());

  return complete(syntacticBase);
}

/// The basic skeleton for building a simple or compound
/// assignment operation.
ExprResult
PseudoOpBuilder::buildAssignmentOperation(Scope *Sc, SourceLocation opcLoc,
                                          BinaryOperatorKind opcode,
                                          Expr *LHS, Expr *RHS) {
  assert(BinaryOperator::isAssignmentOp(opcode));

  Expr *syntacticLHS = rebuildAndCaptureObject(LHS);
  OpaqueValueExpr *capturedRHS = capture(RHS);

  Expr *syntactic;

  ExprResult result;
  if (opcode == BO_Assign) {
    result = capturedRHS;
    syntactic = new (S.Context) BinaryOperator(syntacticLHS, capturedRHS,
                                               opcode, capturedRHS->getType(),
                                               capturedRHS->getValueKind(),
                                               OK_Ordinary, opcLoc);
  } else {
    ExprResult opLHS = buildGet();
    if (opLHS.isInvalid()) return ExprError();

    // Build an ordinary, non-compound operation.
    BinaryOperatorKind nonCompound =
      BinaryOperator::getOpForCompoundAssignment(opcode);
    result = S.BuildBinOp(Sc, opcLoc, nonCompound,
                          opLHS.take(), capturedRHS);
    if (result.isInvalid()) return ExprError();

    syntactic =
      new (S.Context) CompoundAssignOperator(syntacticLHS, capturedRHS, opcode,
                                             result.get()->getType(),
                                             result.get()->getValueKind(),
                                             OK_Ordinary,
                                             opLHS.get()->getType(),
                                             result.get()->getType(),
                                             opcLoc);
  }

  // The result of the assignment, if not void, is the value set into
  // the l-value.
  result = buildSet(result.take(), opcLoc, assignmentsHaveResult());
  if (result.isInvalid()) return ExprError();
  addSemanticExpr(result.take());

  return complete(syntactic);
}

/// The basic skeleton for building an increment or decrement
/// operation.
ExprResult
PseudoOpBuilder::buildIncDecOperation(Scope *Sc, SourceLocation opcLoc,
                                      UnaryOperatorKind opcode,
                                      Expr *op) {
  assert(UnaryOperator::isIncrementDecrementOp(opcode));

  Expr *syntacticOp = rebuildAndCaptureObject(op);

  // Load the value.
  ExprResult result = buildGet();
  if (result.isInvalid()) return ExprError();

  QualType resultType = result.get()->getType();

  // That's the postfix result.
  if (UnaryOperator::isPostfix(opcode) && assignmentsHaveResult()) {
    result = capture(result.take());
    setResultToLastSemantic();
  }

  // Add or subtract a literal 1.
  llvm::APInt oneV(S.Context.getTypeSize(S.Context.IntTy), 1);
  Expr *one = IntegerLiteral::Create(S.Context, oneV, S.Context.IntTy,
                                     GenericLoc);

  if (UnaryOperator::isIncrementOp(opcode)) {
    result = S.BuildBinOp(Sc, opcLoc, BO_Add, result.take(), one);
  } else {
    result = S.BuildBinOp(Sc, opcLoc, BO_Sub, result.take(), one);
  }
  if (result.isInvalid()) return ExprError();

  // Store that back into the result.  The value stored is the result
  // of a prefix operation.
  result = buildSet(result.take(), opcLoc,
             UnaryOperator::isPrefix(opcode) && assignmentsHaveResult());
  if (result.isInvalid()) return ExprError();
  addSemanticExpr(result.take());

  UnaryOperator *syntactic =
    new (S.Context) UnaryOperator(syntacticOp, opcode, resultType,
                                  VK_LValue, OK_Ordinary, opcLoc);
  return complete(syntactic);
}


//===----------------------------------------------------------------------===//
//  Objective-C @property and implicit property references
//===----------------------------------------------------------------------===//

/// Look up a method in the receiver type of an Objective-C property
/// reference.
static ObjCMethodDecl *LookupMethodInReceiverType(Sema &S, Selector sel,
                                            const ObjCPropertyRefExpr *PRE) {
  if (PRE->isObjectReceiver()) {
    const ObjCObjectPointerType *PT =
      PRE->getBase()->getType()->castAs<ObjCObjectPointerType>();

    // Special case for 'self' in class method implementations.
    if (PT->isObjCClassType() &&
        S.isSelfExpr(const_cast<Expr*>(PRE->getBase()))) {
      // This cast is safe because isSelfExpr is only true within
      // methods.
      ObjCMethodDecl *method =
        cast<ObjCMethodDecl>(S.CurContext->getNonClosureAncestor());
      return S.LookupMethodInObjectType(sel,
                 S.Context.getObjCInterfaceType(method->getClassInterface()),
                                        /*instance*/ false);
    }

    return S.LookupMethodInObjectType(sel, PT->getPointeeType(), true);
  }

  if (PRE->isSuperReceiver()) {
    if (const ObjCObjectPointerType *PT =
        PRE->getSuperReceiverType()->getAs<ObjCObjectPointerType>())
      return S.LookupMethodInObjectType(sel, PT->getPointeeType(), true);

    return S.LookupMethodInObjectType(sel, PRE->getSuperReceiverType(), false);
  }

  assert(PRE->isClassReceiver() && "Invalid expression");
  QualType IT = S.Context.getObjCInterfaceType(PRE->getClassReceiver());
  return S.LookupMethodInObjectType(sel, IT, false);
}

bool ObjCPropertyOpBuilder::findGetter() {
  if (Getter) return true;

  // For implicit properties, just trust the lookup we already did.
  if (RefExpr->isImplicitProperty()) {
    Getter = RefExpr->getImplicitPropertyGetter();
    return (Getter != 0);
  }

  ObjCPropertyDecl *prop = RefExpr->getExplicitProperty();
  Getter = LookupMethodInReceiverType(S, prop->getGetterName(), RefExpr);
  return (Getter != 0);
}

/// Try to find the most accurate setter declaration for the property
/// reference.
///
/// \return true if a setter was found, in which case Setter 
bool ObjCPropertyOpBuilder::findSetter() {
  // For implicit properties, just trust the lookup we already did.
  if (RefExpr->isImplicitProperty()) {
    if (ObjCMethodDecl *setter = RefExpr->getImplicitPropertySetter()) {
      Setter = setter;
      SetterSelector = setter->getSelector();
      return true;
    } else {
      IdentifierInfo *getterName =
        RefExpr->getImplicitPropertyGetter()->getSelector()
          .getIdentifierInfoForSlot(0);
      SetterSelector =
        SelectorTable::constructSetterName(S.PP.getIdentifierTable(),
                                           S.PP.getSelectorTable(),
                                           getterName);
      return false;
    }
  }

  // For explicit properties, this is more involved.
  ObjCPropertyDecl *prop = RefExpr->getExplicitProperty();
  SetterSelector = prop->getSetterName();

  // Do a normal method lookup first.
  if (ObjCMethodDecl *setter =
        LookupMethodInReceiverType(S, SetterSelector, RefExpr)) {
    Setter = setter;
    return true;
  }

  // That can fail in the somewhat crazy situation that we're
  // type-checking a message send within the @interface declaration
  // that declared the @property.  But it's not clear that that's
  // valuable to support.

  return false;
}

/// Capture the base object of an Objective-C property expression.
Expr *ObjCPropertyOpBuilder::rebuildAndCaptureObject(Expr *syntacticBase) {
  assert(InstanceReceiver == 0);

  // If we have a base, capture it in an OVE and rebuild the syntactic
  // form to use the OVE as its base.
  if (RefExpr->isObjectReceiver()) {
    InstanceReceiver = capture(RefExpr->getBase());

    syntacticBase =
      ObjCPropertyRefRebuilder(S, InstanceReceiver).rebuild(syntacticBase);
  }

  if (ObjCPropertyRefExpr *
        refE = dyn_cast<ObjCPropertyRefExpr>(syntacticBase->IgnoreParens()))
    SyntacticRefExpr = refE;

  return syntacticBase;
}

/// Load from an Objective-C property reference.
ExprResult ObjCPropertyOpBuilder::buildGet() {
  findGetter();
  assert(Getter);

  if (SyntacticRefExpr)
    SyntacticRefExpr->setIsMessagingGetter();

  QualType receiverType;
  if (RefExpr->isClassReceiver()) {
    receiverType = S.Context.getObjCInterfaceType(RefExpr->getClassReceiver());
  } else if (RefExpr->isSuperReceiver()) {
    receiverType = RefExpr->getSuperReceiverType();
  } else {
    assert(InstanceReceiver);
    receiverType = InstanceReceiver->getType();
  }

  // Build a message-send.
  ExprResult msg;
  if (Getter->isInstanceMethod() || RefExpr->isObjectReceiver()) {
    assert(InstanceReceiver || RefExpr->isSuperReceiver());
    msg = S.BuildInstanceMessageImplicit(InstanceReceiver, receiverType,
                                         GenericLoc, Getter->getSelector(),
                                         Getter, MultiExprArg());
  } else {
    msg = S.BuildClassMessageImplicit(receiverType, RefExpr->isSuperReceiver(),
                                      GenericLoc,
                                      Getter->getSelector(), Getter,
                                      MultiExprArg());
  }
  return msg;
}

/// Store to an Objective-C property reference.
///
/// \param bindSetValueAsResult - If true, capture the actual
///   value being set as the value of the property operation.
ExprResult ObjCPropertyOpBuilder::buildSet(Expr *op, SourceLocation opcLoc,
                                           bool captureSetValueAsResult) {
  bool hasSetter = findSetter();
  assert(hasSetter); (void) hasSetter;

  if (SyntacticRefExpr)
    SyntacticRefExpr->setIsMessagingSetter();

  QualType receiverType;
  if (RefExpr->isClassReceiver()) {
    receiverType = S.Context.getObjCInterfaceType(RefExpr->getClassReceiver());
  } else if (RefExpr->isSuperReceiver()) {
    receiverType = RefExpr->getSuperReceiverType();
  } else {
    assert(InstanceReceiver);
    receiverType = InstanceReceiver->getType();
  }

  // Use assignment constraints when possible; they give us better
  // diagnostics.  "When possible" basically means anything except a
  // C++ class type.
  if (!S.getLangOpts().CPlusPlus || !op->getType()->isRecordType()) {
    QualType paramType = (*Setter->param_begin())->getType();
    if (!S.getLangOpts().CPlusPlus || !paramType->isRecordType()) {
      ExprResult opResult = op;
      Sema::AssignConvertType assignResult
        = S.CheckSingleAssignmentConstraints(paramType, opResult);
      if (S.DiagnoseAssignmentResult(assignResult, opcLoc, paramType,
                                     op->getType(), opResult.get(),
                                     Sema::AA_Assigning))
        return ExprError();

      op = opResult.take();
      assert(op && "successful assignment left argument invalid?");
    }
  }

  // Arguments.
  Expr *args[] = { op };

  // Build a message-send.
  ExprResult msg;
  if (Setter->isInstanceMethod() || RefExpr->isObjectReceiver()) {
    msg = S.BuildInstanceMessageImplicit(InstanceReceiver, receiverType,
                                         GenericLoc, SetterSelector, Setter,
                                         MultiExprArg(args, 1));
  } else {
    msg = S.BuildClassMessageImplicit(receiverType, RefExpr->isSuperReceiver(),
                                      GenericLoc,
                                      SetterSelector, Setter,
                                      MultiExprArg(args, 1));
  }

  if (!msg.isInvalid() && captureSetValueAsResult) {
    ObjCMessageExpr *msgExpr =
      cast<ObjCMessageExpr>(msg.get()->IgnoreImplicit());
    Expr *arg = msgExpr->getArg(0);
    msgExpr->setArg(0, captureValueAsResult(arg));
  }

  return msg;
}

/// @property-specific behavior for doing lvalue-to-rvalue conversion.
ExprResult ObjCPropertyOpBuilder::buildRValueOperation(Expr *op) {
  // Explicit properties always have getters, but implicit ones don't.
  // Check that before proceeding.
  if (RefExpr->isImplicitProperty() &&
      !RefExpr->getImplicitPropertyGetter()) {
    S.Diag(RefExpr->getLocation(), diag::err_getter_not_found)
      << RefExpr->getBase()->getType();
    return ExprError();
  }

  ExprResult result = PseudoOpBuilder::buildRValueOperation(op);
  if (result.isInvalid()) return ExprError();

  if (RefExpr->isExplicitProperty() && !Getter->hasRelatedResultType())
    S.DiagnosePropertyAccessorMismatch(RefExpr->getExplicitProperty(),
                                       Getter, RefExpr->getLocation());

  // As a special case, if the method returns 'id', try to get
  // a better type from the property.
  if (RefExpr->isExplicitProperty() && result.get()->isRValue() &&
      result.get()->getType()->isObjCIdType()) {
    QualType propType = RefExpr->getExplicitProperty()->getType();
    if (const ObjCObjectPointerType *ptr
          = propType->getAs<ObjCObjectPointerType>()) {
      if (!ptr->isObjCIdType())
        result = S.ImpCastExprToType(result.get(), propType, CK_BitCast);
    }
  }

  return result;
}

/// Try to build this as a call to a getter that returns a reference.
///
/// \return true if it was possible, whether or not it actually
///   succeeded
bool ObjCPropertyOpBuilder::tryBuildGetOfReference(Expr *op,
                                                   ExprResult &result) {
  if (!S.getLangOpts().CPlusPlus) return false;

  findGetter();
  assert(Getter && "property has no setter and no getter!");

  // Only do this if the getter returns an l-value reference type.
  QualType resultType = Getter->getResultType();
  if (!resultType->isLValueReferenceType()) return false;

  result = buildRValueOperation(op);
  return true;
}

/// @property-specific behavior for doing assignments.
ExprResult
ObjCPropertyOpBuilder::buildAssignmentOperation(Scope *Sc,
                                                SourceLocation opcLoc,
                                                BinaryOperatorKind opcode,
                                                Expr *LHS, Expr *RHS) {
  assert(BinaryOperator::isAssignmentOp(opcode));

  // If there's no setter, we have no choice but to try to assign to
  // the result of the getter.
  if (!findSetter()) {
    ExprResult result;
    if (tryBuildGetOfReference(LHS, result)) {
      if (result.isInvalid()) return ExprError();
      return S.BuildBinOp(Sc, opcLoc, opcode, result.take(), RHS);
    }

    // Otherwise, it's an error.
    S.Diag(opcLoc, diag::err_nosetter_property_assignment)
      << unsigned(RefExpr->isImplicitProperty())
      << SetterSelector
      << LHS->getSourceRange() << RHS->getSourceRange();
    return ExprError();
  }

  // If there is a setter, we definitely want to use it.

  // Verify that we can do a compound assignment.
  if (opcode != BO_Assign && !findGetter()) {
    S.Diag(opcLoc, diag::err_nogetter_property_compound_assignment)
      << LHS->getSourceRange() << RHS->getSourceRange();
    return ExprError();
  }

  ExprResult result =
    PseudoOpBuilder::buildAssignmentOperation(Sc, opcLoc, opcode, LHS, RHS);
  if (result.isInvalid()) return ExprError();

  // Various warnings about property assignments in ARC.
  if (S.getLangOpts().ObjCAutoRefCount && InstanceReceiver) {
    S.checkRetainCycles(InstanceReceiver->getSourceExpr(), RHS);
    S.checkUnsafeExprAssigns(opcLoc, LHS, RHS);
  }

  return result;
}

/// @property-specific behavior for doing increments and decrements.
ExprResult
ObjCPropertyOpBuilder::buildIncDecOperation(Scope *Sc, SourceLocation opcLoc,
                                            UnaryOperatorKind opcode,
                                            Expr *op) {
  // If there's no setter, we have no choice but to try to assign to
  // the result of the getter.
  if (!findSetter()) {
    ExprResult result;
    if (tryBuildGetOfReference(op, result)) {
      if (result.isInvalid()) return ExprError();
      return S.BuildUnaryOp(Sc, opcLoc, opcode, result.take());
    }

    // Otherwise, it's an error.
    S.Diag(opcLoc, diag::err_nosetter_property_incdec)
      << unsigned(RefExpr->isImplicitProperty())
      << unsigned(UnaryOperator::isDecrementOp(opcode))
      << SetterSelector
      << op->getSourceRange();
    return ExprError();
  }

  // If there is a setter, we definitely want to use it.

  // We also need a getter.
  if (!findGetter()) {
    assert(RefExpr->isImplicitProperty());
    S.Diag(opcLoc, diag::err_nogetter_property_incdec)
      << unsigned(UnaryOperator::isDecrementOp(opcode))
      << RefExpr->getImplicitPropertyGetter()->getSelector() // FIXME!
      << op->getSourceRange();
    return ExprError();
  }

  return PseudoOpBuilder::buildIncDecOperation(Sc, opcLoc, opcode, op);
}

// ObjCSubscript build stuff.
//

/// objective-c subscripting-specific behavior for doing lvalue-to-rvalue 
/// conversion.
/// FIXME. Remove this routine if it is proven that no additional 
/// specifity is needed.
ExprResult ObjCSubscriptOpBuilder::buildRValueOperation(Expr *op) {
  ExprResult result = PseudoOpBuilder::buildRValueOperation(op);
  if (result.isInvalid()) return ExprError();
  return result;
}

/// objective-c subscripting-specific  behavior for doing assignments.
ExprResult
ObjCSubscriptOpBuilder::buildAssignmentOperation(Scope *Sc,
                                                SourceLocation opcLoc,
                                                BinaryOperatorKind opcode,
                                                Expr *LHS, Expr *RHS) {
  assert(BinaryOperator::isAssignmentOp(opcode));
  // There must be a method to do the Index'ed assignment.
  if (!findAtIndexSetter())
    return ExprError();
  
  // Verify that we can do a compound assignment.
  if (opcode != BO_Assign && !findAtIndexGetter())
    return ExprError();
  
  ExprResult result =
  PseudoOpBuilder::buildAssignmentOperation(Sc, opcLoc, opcode, LHS, RHS);
  if (result.isInvalid()) return ExprError();
  
  // Various warnings about objc Index'ed assignments in ARC.
  if (S.getLangOpts().ObjCAutoRefCount && InstanceBase) {
    S.checkRetainCycles(InstanceBase->getSourceExpr(), RHS);
    S.checkUnsafeExprAssigns(opcLoc, LHS, RHS);
  }
  
  return result;
}

/// Capture the base object of an Objective-C Index'ed expression.
Expr *ObjCSubscriptOpBuilder::rebuildAndCaptureObject(Expr *syntacticBase) {
  assert(InstanceBase == 0);
  
  // Capture base expression in an OVE and rebuild the syntactic
  // form to use the OVE as its base expression.
  InstanceBase = capture(RefExpr->getBaseExpr());
  InstanceKey = capture(RefExpr->getKeyExpr());
    
  syntacticBase =
    ObjCSubscriptRefRebuilder(S, InstanceBase, 
                              InstanceKey).rebuild(syntacticBase);
  
  return syntacticBase;
}

/// CheckSubscriptingKind - This routine decide what type 
/// of indexing represented by "FromE" is being done.
Sema::ObjCSubscriptKind 
  Sema::CheckSubscriptingKind(Expr *FromE) {
  // If the expression already has integral or enumeration type, we're golden.
  QualType T = FromE->getType();
  if (T->isIntegralOrEnumerationType())
    return OS_Array;
  
  // If we don't have a class type in C++, there's no way we can get an
  // expression of integral or enumeration type.
  const RecordType *RecordTy = T->getAs<RecordType>();
  if (!RecordTy && T->isObjCObjectPointerType())
    // All other scalar cases are assumed to be dictionary indexing which
    // caller handles, with diagnostics if needed.
    return OS_Dictionary;
  if (!getLangOpts().CPlusPlus || 
      !RecordTy || RecordTy->isIncompleteType()) {
    // No indexing can be done. Issue diagnostics and quit.
    const Expr *IndexExpr = FromE->IgnoreParenImpCasts();
    if (isa<StringLiteral>(IndexExpr))
      Diag(FromE->getExprLoc(), diag::err_objc_subscript_pointer)
        << T << FixItHint::CreateInsertion(FromE->getExprLoc(), "@");
    else
      Diag(FromE->getExprLoc(), diag::err_objc_subscript_type_conversion)
        << T;
    return OS_Error;
  }
  
  // We must have a complete class type.
  if (RequireCompleteType(FromE->getExprLoc(), T, 
                          PDiag(diag::err_objc_index_incomplete_class_type)
                          << FromE->getSourceRange()))
    return OS_Error;
  
  // Look for a conversion to an integral, enumeration type, or
  // objective-C pointer type.
  UnresolvedSet<4> ViableConversions;
  UnresolvedSet<4> ExplicitConversions;
  const UnresolvedSetImpl *Conversions
    = cast<CXXRecordDecl>(RecordTy->getDecl())->getVisibleConversionFunctions();
  
  int NoIntegrals=0, NoObjCIdPointers=0;
  SmallVector<CXXConversionDecl *, 4> ConversionDecls;
    
  for (UnresolvedSetImpl::iterator I = Conversions->begin(),
       E = Conversions->end();
       I != E;
       ++I) {
    if (CXXConversionDecl *Conversion
        = dyn_cast<CXXConversionDecl>((*I)->getUnderlyingDecl())) {
      QualType CT = Conversion->getConversionType().getNonReferenceType();
      if (CT->isIntegralOrEnumerationType()) {
        ++NoIntegrals;
        ConversionDecls.push_back(Conversion);
      }
      else if (CT->isObjCIdType() ||CT->isBlockPointerType()) {
        ++NoObjCIdPointers;
        ConversionDecls.push_back(Conversion);
      }
    }
  }
  if (NoIntegrals ==1 && NoObjCIdPointers == 0)
    return OS_Array;
  if (NoIntegrals == 0 && NoObjCIdPointers == 1)
    return OS_Dictionary;
  if (NoIntegrals == 0 && NoObjCIdPointers == 0) {
    // No conversion function was found. Issue diagnostic and return.
    Diag(FromE->getExprLoc(), diag::err_objc_subscript_type_conversion)
      << FromE->getType();
    return OS_Error;
  }
  Diag(FromE->getExprLoc(), diag::err_objc_multiple_subscript_type_conversion)
      << FromE->getType();
  for (unsigned int i = 0; i < ConversionDecls.size(); i++)
    Diag(ConversionDecls[i]->getLocation(), diag::not_conv_function_declared_at);
    
  return OS_Error;
}

bool ObjCSubscriptOpBuilder::findAtIndexGetter() {
  if (AtIndexGetter)
    return true;
  
  Expr *BaseExpr = RefExpr->getBaseExpr();
  QualType BaseT = BaseExpr->getType();
  
  QualType ResultType;
  if (const ObjCObjectPointerType *PTy =
      BaseT->getAs<ObjCObjectPointerType>()) {
    ResultType = PTy->getPointeeType();
    if (const ObjCObjectType *iQFaceTy = 
        ResultType->getAsObjCQualifiedInterfaceType())
      ResultType = iQFaceTy->getBaseType();
  }
  Sema::ObjCSubscriptKind Res = 
    S.CheckSubscriptingKind(RefExpr->getKeyExpr());
  if (Res == Sema::OS_Error)
    return false;
  bool arrayRef = (Res == Sema::OS_Array);
  
  if (ResultType.isNull()) {
    S.Diag(BaseExpr->getExprLoc(), diag::err_objc_subscript_base_type)
      << BaseExpr->getType() << arrayRef;
    return false;
  }
  if (!arrayRef) {
    // dictionary subscripting.
    // - (id)objectForKeyedSubscript:(id)key;
    IdentifierInfo *KeyIdents[] = {
      &S.Context.Idents.get("objectForKeyedSubscript")  
    };
    AtIndexGetterSelector = S.Context.Selectors.getSelector(1, KeyIdents);
  }
  else {
    // - (id)objectAtIndexedSubscript:(size_t)index;
    IdentifierInfo *KeyIdents[] = {
      &S.Context.Idents.get("objectAtIndexedSubscript")  
    };
  
    AtIndexGetterSelector = S.Context.Selectors.getSelector(1, KeyIdents);
  }
  
  AtIndexGetter = S.LookupMethodInObjectType(AtIndexGetterSelector, ResultType, 
                                             true /*instance*/);
  bool receiverIdType = (BaseT->isObjCIdType() ||
                         BaseT->isObjCQualifiedIdType());
  
  if (!AtIndexGetter && S.getLangOpts().DebuggerObjCLiteral) {
    AtIndexGetter = ObjCMethodDecl::Create(S.Context, SourceLocation(), 
                           SourceLocation(), AtIndexGetterSelector,
                           S.Context.getObjCIdType() /*ReturnType*/,
                           0 /*TypeSourceInfo */,
                           S.Context.getTranslationUnitDecl(),
                           true /*Instance*/, false/*isVariadic*/,
                           /*isSynthesized=*/false,
                           /*isImplicitlyDeclared=*/true, /*isDefined=*/false,
                           ObjCMethodDecl::Required,
                           false);
    ParmVarDecl *Argument = ParmVarDecl::Create(S.Context, AtIndexGetter,
                                                SourceLocation(), SourceLocation(),
                                                arrayRef ? &S.Context.Idents.get("index")
                                                         : &S.Context.Idents.get("key"),
                                                arrayRef ? S.Context.UnsignedLongTy
                                                         : S.Context.getObjCIdType(),
                                                /*TInfo=*/0,
                                                SC_None,
                                                SC_None,
                                                0);
    AtIndexGetter->setMethodParams(S.Context, Argument, 
                                   ArrayRef<SourceLocation>());
  }

  if (!AtIndexGetter) {
    if (!receiverIdType) {
      S.Diag(BaseExpr->getExprLoc(), diag::err_objc_subscript_method_not_found)
      << BaseExpr->getType() << 0 << arrayRef;
      return false;
    }
    AtIndexGetter = 
      S.LookupInstanceMethodInGlobalPool(AtIndexGetterSelector, 
                                         RefExpr->getSourceRange(), 
                                         true, false);
  }
  
  if (AtIndexGetter) {
    QualType T = AtIndexGetter->param_begin()[0]->getType();
    if ((arrayRef && !T->isIntegralOrEnumerationType()) ||
        (!arrayRef && !T->isObjCObjectPointerType())) {
      S.Diag(RefExpr->getKeyExpr()->getExprLoc(), 
             arrayRef ? diag::err_objc_subscript_index_type
                      : diag::err_objc_subscript_key_type) << T;
      S.Diag(AtIndexGetter->param_begin()[0]->getLocation(), 
             diag::note_parameter_type) << T;
      return false;
    }
    QualType R = AtIndexGetter->getResultType();
    if (!R->isObjCObjectPointerType()) {
      S.Diag(RefExpr->getKeyExpr()->getExprLoc(),
             diag::err_objc_indexing_method_result_type) << R << arrayRef;
      S.Diag(AtIndexGetter->getLocation(), diag::note_method_declared_at) <<
        AtIndexGetter->getDeclName();
    }
  }
  return true;
}

bool ObjCSubscriptOpBuilder::findAtIndexSetter() {
  if (AtIndexSetter)
    return true;
  
  Expr *BaseExpr = RefExpr->getBaseExpr();
  QualType BaseT = BaseExpr->getType();
  
  QualType ResultType;
  if (const ObjCObjectPointerType *PTy =
      BaseT->getAs<ObjCObjectPointerType>()) {
    ResultType = PTy->getPointeeType();
    if (const ObjCObjectType *iQFaceTy = 
        ResultType->getAsObjCQualifiedInterfaceType())
      ResultType = iQFaceTy->getBaseType();
  }
  
  Sema::ObjCSubscriptKind Res = 
    S.CheckSubscriptingKind(RefExpr->getKeyExpr());
  if (Res == Sema::OS_Error)
    return false;
  bool arrayRef = (Res == Sema::OS_Array);
  
  if (ResultType.isNull()) {
    S.Diag(BaseExpr->getExprLoc(), diag::err_objc_subscript_base_type)
      << BaseExpr->getType() << arrayRef;
    return false;
  }
  
  if (!arrayRef) {
    // dictionary subscripting.
    // - (void)setObject:(id)object forKeyedSubscript:(id)key;
    IdentifierInfo *KeyIdents[] = {
      &S.Context.Idents.get("setObject"),
      &S.Context.Idents.get("forKeyedSubscript")
    };
    AtIndexSetterSelector = S.Context.Selectors.getSelector(2, KeyIdents);
  }
  else {
    // - (void)setObject:(id)object atIndexedSubscript:(NSInteger)index;
    IdentifierInfo *KeyIdents[] = {
      &S.Context.Idents.get("setObject"),
      &S.Context.Idents.get("atIndexedSubscript")
    };
    AtIndexSetterSelector = S.Context.Selectors.getSelector(2, KeyIdents);
  }
  AtIndexSetter = S.LookupMethodInObjectType(AtIndexSetterSelector, ResultType, 
                                             true /*instance*/);
  
  bool receiverIdType = (BaseT->isObjCIdType() ||
                         BaseT->isObjCQualifiedIdType());

  if (!AtIndexSetter && S.getLangOpts().DebuggerObjCLiteral) {
    TypeSourceInfo *ResultTInfo = 0;
    QualType ReturnType = S.Context.VoidTy;
    AtIndexSetter = ObjCMethodDecl::Create(S.Context, SourceLocation(),
                           SourceLocation(), AtIndexSetterSelector,
                           ReturnType,
                           ResultTInfo,
                           S.Context.getTranslationUnitDecl(),
                           true /*Instance*/, false/*isVariadic*/,
                           /*isSynthesized=*/false,
                           /*isImplicitlyDeclared=*/true, /*isDefined=*/false,
                           ObjCMethodDecl::Required,
                           false); 
    SmallVector<ParmVarDecl *, 2> Params;
    ParmVarDecl *object = ParmVarDecl::Create(S.Context, AtIndexSetter,
                                                SourceLocation(), SourceLocation(),
                                                &S.Context.Idents.get("object"),
                                                S.Context.getObjCIdType(),
                                                /*TInfo=*/0,
                                                SC_None,
                                                SC_None,
                                                0);
    Params.push_back(object);
    ParmVarDecl *key = ParmVarDecl::Create(S.Context, AtIndexSetter,
                                                SourceLocation(), SourceLocation(),
                                                arrayRef ?  &S.Context.Idents.get("index")
                                                         :  &S.Context.Idents.get("key"),
                                                arrayRef ? S.Context.UnsignedLongTy
                                                         : S.Context.getObjCIdType(),
                                                /*TInfo=*/0,
                                                SC_None,
                                                SC_None,
                                                0);
    Params.push_back(key);
    AtIndexSetter->setMethodParams(S.Context, Params, ArrayRef<SourceLocation>());
  }
  
  if (!AtIndexSetter) {
    if (!receiverIdType) {
      S.Diag(BaseExpr->getExprLoc(), 
             diag::err_objc_subscript_method_not_found)
      << BaseExpr->getType() << 1 << arrayRef;
      return false;
    }
    AtIndexSetter = 
      S.LookupInstanceMethodInGlobalPool(AtIndexSetterSelector, 
                                         RefExpr->getSourceRange(), 
                                         true, false);
  }
  
  bool err = false;
  if (AtIndexSetter && arrayRef) {
    QualType T = AtIndexSetter->param_begin()[1]->getType();
    if (!T->isIntegralOrEnumerationType()) {
      S.Diag(RefExpr->getKeyExpr()->getExprLoc(), 
             diag::err_objc_subscript_index_type) << T;
      S.Diag(AtIndexSetter->param_begin()[1]->getLocation(), 
             diag::note_parameter_type) << T;
      err = true;
    }
    T = AtIndexSetter->param_begin()[0]->getType();
    if (!T->isObjCObjectPointerType()) {
      S.Diag(RefExpr->getBaseExpr()->getExprLoc(), 
             diag::err_objc_subscript_object_type) << T << arrayRef;
      S.Diag(AtIndexSetter->param_begin()[0]->getLocation(), 
             diag::note_parameter_type) << T;
      err = true;
    }
  }
  else if (AtIndexSetter && !arrayRef)
    for (unsigned i=0; i <2; i++) {
      QualType T = AtIndexSetter->param_begin()[i]->getType();
      if (!T->isObjCObjectPointerType()) {
        if (i == 1)
          S.Diag(RefExpr->getKeyExpr()->getExprLoc(),
                 diag::err_objc_subscript_key_type) << T;
        else
          S.Diag(RefExpr->getBaseExpr()->getExprLoc(),
                 diag::err_objc_subscript_dic_object_type) << T;
        S.Diag(AtIndexSetter->param_begin()[i]->getLocation(), 
               diag::note_parameter_type) << T;
        err = true;
      }
    }

  return !err;
}

// Get the object at "Index" position in the container.
// [BaseExpr objectAtIndexedSubscript : IndexExpr];
ExprResult ObjCSubscriptOpBuilder::buildGet() {
  if (!findAtIndexGetter())
    return ExprError();
  
  QualType receiverType = InstanceBase->getType();
    
  // Build a message-send.
  ExprResult msg;
  Expr *Index = InstanceKey;
  
  // Arguments.
  Expr *args[] = { Index };
  assert(InstanceBase);
  msg = S.BuildInstanceMessageImplicit(InstanceBase, receiverType,
                                       GenericLoc,
                                       AtIndexGetterSelector, AtIndexGetter,
                                       MultiExprArg(args, 1));
  return msg;
}

/// Store into the container the "op" object at "Index"'ed location
/// by building this messaging expression:
/// - (void)setObject:(id)object atIndexedSubscript:(NSInteger)index;
/// \param bindSetValueAsResult - If true, capture the actual
///   value being set as the value of the property operation.
ExprResult ObjCSubscriptOpBuilder::buildSet(Expr *op, SourceLocation opcLoc,
                                           bool captureSetValueAsResult) {
  if (!findAtIndexSetter())
    return ExprError();
  
  QualType receiverType = InstanceBase->getType();
  Expr *Index = InstanceKey;
  
  // Arguments.
  Expr *args[] = { op, Index };
  
  // Build a message-send.
  ExprResult msg = S.BuildInstanceMessageImplicit(InstanceBase, receiverType,
                                                  GenericLoc,
                                                  AtIndexSetterSelector,
                                                  AtIndexSetter,
                                                  MultiExprArg(args, 2));
  
  if (!msg.isInvalid() && captureSetValueAsResult) {
    ObjCMessageExpr *msgExpr =
      cast<ObjCMessageExpr>(msg.get()->IgnoreImplicit());
    Expr *arg = msgExpr->getArg(0);
    msgExpr->setArg(0, captureValueAsResult(arg));
  }
  
  return msg;
}

//===----------------------------------------------------------------------===//
//  General Sema routines.
//===----------------------------------------------------------------------===//

ExprResult Sema::checkPseudoObjectRValue(Expr *E) {
  Expr *opaqueRef = E->IgnoreParens();
  if (ObjCPropertyRefExpr *refExpr
        = dyn_cast<ObjCPropertyRefExpr>(opaqueRef)) {
    ObjCPropertyOpBuilder builder(*this, refExpr);
    return builder.buildRValueOperation(E);
  }
  else if (ObjCSubscriptRefExpr *refExpr
           = dyn_cast<ObjCSubscriptRefExpr>(opaqueRef)) {
    ObjCSubscriptOpBuilder builder(*this, refExpr);
    return builder.buildRValueOperation(E);
  } else {
    llvm_unreachable("unknown pseudo-object kind!");
  }
}

/// Check an increment or decrement of a pseudo-object expression.
ExprResult Sema::checkPseudoObjectIncDec(Scope *Sc, SourceLocation opcLoc,
                                         UnaryOperatorKind opcode, Expr *op) {
  // Do nothing if the operand is dependent.
  if (op->isTypeDependent())
    return new (Context) UnaryOperator(op, opcode, Context.DependentTy,
                                       VK_RValue, OK_Ordinary, opcLoc);

  assert(UnaryOperator::isIncrementDecrementOp(opcode));
  Expr *opaqueRef = op->IgnoreParens();
  if (ObjCPropertyRefExpr *refExpr
        = dyn_cast<ObjCPropertyRefExpr>(opaqueRef)) {
    ObjCPropertyOpBuilder builder(*this, refExpr);
    return builder.buildIncDecOperation(Sc, opcLoc, opcode, op);
  } else if (isa<ObjCSubscriptRefExpr>(opaqueRef)) {
    Diag(opcLoc, diag::err_illegal_container_subscripting_op);
    return ExprError();
  } else {
    llvm_unreachable("unknown pseudo-object kind!");
  }
}

ExprResult Sema::checkPseudoObjectAssignment(Scope *S, SourceLocation opcLoc,
                                             BinaryOperatorKind opcode,
                                             Expr *LHS, Expr *RHS) {
  // Do nothing if either argument is dependent.
  if (LHS->isTypeDependent() || RHS->isTypeDependent())
    return new (Context) BinaryOperator(LHS, RHS, opcode, Context.DependentTy,
                                        VK_RValue, OK_Ordinary, opcLoc);

  // Filter out non-overload placeholder types in the RHS.
  if (RHS->getType()->isNonOverloadPlaceholderType()) {
    ExprResult result = CheckPlaceholderExpr(RHS);
    if (result.isInvalid()) return ExprError();
    RHS = result.take();
  }

  Expr *opaqueRef = LHS->IgnoreParens();
  if (ObjCPropertyRefExpr *refExpr
        = dyn_cast<ObjCPropertyRefExpr>(opaqueRef)) {
    ObjCPropertyOpBuilder builder(*this, refExpr);
    return builder.buildAssignmentOperation(S, opcLoc, opcode, LHS, RHS);
  } else if (ObjCSubscriptRefExpr *refExpr
             = dyn_cast<ObjCSubscriptRefExpr>(opaqueRef)) {
    ObjCSubscriptOpBuilder builder(*this, refExpr);
    return builder.buildAssignmentOperation(S, opcLoc, opcode, LHS, RHS);
  } else {
    llvm_unreachable("unknown pseudo-object kind!");
  }
}

/// Given a pseudo-object reference, rebuild it without the opaque
/// values.  Basically, undo the behavior of rebuildAndCaptureObject.
/// This should never operate in-place.
static Expr *stripOpaqueValuesFromPseudoObjectRef(Sema &S, Expr *E) {
  Expr *opaqueRef = E->IgnoreParens();
  if (ObjCPropertyRefExpr *refExpr
        = dyn_cast<ObjCPropertyRefExpr>(opaqueRef)) {
    OpaqueValueExpr *baseOVE = cast<OpaqueValueExpr>(refExpr->getBase());
    return ObjCPropertyRefRebuilder(S, baseOVE->getSourceExpr()).rebuild(E);
  } else if (ObjCSubscriptRefExpr *refExpr
               = dyn_cast<ObjCSubscriptRefExpr>(opaqueRef)) {
    OpaqueValueExpr *baseOVE = cast<OpaqueValueExpr>(refExpr->getBaseExpr());
    OpaqueValueExpr *keyOVE = cast<OpaqueValueExpr>(refExpr->getKeyExpr());
    return ObjCSubscriptRefRebuilder(S, baseOVE->getSourceExpr(), 
                                     keyOVE->getSourceExpr()).rebuild(E);
  } else {
    llvm_unreachable("unknown pseudo-object kind!");
  }
}

/// Given a pseudo-object expression, recreate what it looks like
/// syntactically without the attendant OpaqueValueExprs.
///
/// This is a hack which should be removed when TreeTransform is
/// capable of rebuilding a tree without stripping implicit
/// operations.
Expr *Sema::recreateSyntacticForm(PseudoObjectExpr *E) {
  Expr *syntax = E->getSyntacticForm();
  if (UnaryOperator *uop = dyn_cast<UnaryOperator>(syntax)) {
    Expr *op = stripOpaqueValuesFromPseudoObjectRef(*this, uop->getSubExpr());
    return new (Context) UnaryOperator(op, uop->getOpcode(), uop->getType(),
                                       uop->getValueKind(), uop->getObjectKind(),
                                       uop->getOperatorLoc());
  } else if (CompoundAssignOperator *cop
               = dyn_cast<CompoundAssignOperator>(syntax)) {
    Expr *lhs = stripOpaqueValuesFromPseudoObjectRef(*this, cop->getLHS());
    Expr *rhs = cast<OpaqueValueExpr>(cop->getRHS())->getSourceExpr();
    return new (Context) CompoundAssignOperator(lhs, rhs, cop->getOpcode(),
                                                cop->getType(),
                                                cop->getValueKind(),
                                                cop->getObjectKind(),
                                                cop->getComputationLHSType(),
                                                cop->getComputationResultType(),
                                                cop->getOperatorLoc());
  } else if (BinaryOperator *bop = dyn_cast<BinaryOperator>(syntax)) {
    Expr *lhs = stripOpaqueValuesFromPseudoObjectRef(*this, bop->getLHS());
    Expr *rhs = cast<OpaqueValueExpr>(bop->getRHS())->getSourceExpr();
    return new (Context) BinaryOperator(lhs, rhs, bop->getOpcode(),
                                        bop->getType(), bop->getValueKind(),
                                        bop->getObjectKind(),
                                        bop->getOperatorLoc());
  } else {
    assert(syntax->hasPlaceholderType(BuiltinType::PseudoObject));
    return stripOpaqueValuesFromPseudoObjectRef(*this, syntax);
  }
}
