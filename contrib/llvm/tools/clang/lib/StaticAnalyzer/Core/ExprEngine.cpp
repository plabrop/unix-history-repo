//=-- ExprEngine.cpp - Path-Sensitive Expression-Level Dataflow ---*- C++ -*-=
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines a meta-engine for path-sensitive dataflow analysis that
//  is built on GREngine, but provides the boilerplate to execute transfer
//  functions and build the ExplodedGraph at the expression level.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "ExprEngine"

#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/StmtObjC.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/ImmutableList.h"
#include "llvm/ADT/Statistic.h"

#ifndef NDEBUG
#include "llvm/Support/GraphWriter.h"
#endif

using namespace clang;
using namespace ento;
using llvm::APSInt;

STATISTIC(NumRemoveDeadBindings,
            "The # of times RemoveDeadBindings is called");
STATISTIC(NumMaxBlockCountReached,
            "The # of aborted paths due to reaching the maximum block count in "
            "a top level function");
STATISTIC(NumMaxBlockCountReachedInInlined,
            "The # of aborted paths due to reaching the maximum block count in "
            "an inlined function");
STATISTIC(NumTimesRetriedWithoutInlining,
            "The # of times we re-evaluated a call without inlining");

//===----------------------------------------------------------------------===//
// Engine construction and deletion.
//===----------------------------------------------------------------------===//

ExprEngine::ExprEngine(AnalysisManager &mgr, bool gcEnabled,
                       SetOfConstDecls *VisitedCalleesIn,
                       FunctionSummariesTy *FS)
  : AMgr(mgr),
    AnalysisDeclContexts(mgr.getAnalysisDeclContextManager()),
    Engine(*this, FS),
    G(Engine.getGraph()),
    StateMgr(getContext(), mgr.getStoreManagerCreator(),
             mgr.getConstraintManagerCreator(), G.getAllocator(),
             this),
    SymMgr(StateMgr.getSymbolManager()),
    svalBuilder(StateMgr.getSValBuilder()),
    EntryNode(NULL),
    currStmt(NULL), currStmtIdx(0), currBldrCtx(0),
    ObjCNoRet(mgr.getASTContext()),
    ObjCGCEnabled(gcEnabled), BR(mgr, *this),
    VisitedCallees(VisitedCalleesIn)
{
  unsigned TrimInterval = mgr.options.getGraphTrimInterval();
  if (TrimInterval != 0) {
    // Enable eager node reclaimation when constructing the ExplodedGraph.
    G.enableNodeReclamation(TrimInterval);
  }
}

ExprEngine::~ExprEngine() {
  BR.FlushReports();
}

//===----------------------------------------------------------------------===//
// Utility methods.
//===----------------------------------------------------------------------===//

ProgramStateRef ExprEngine::getInitialState(const LocationContext *InitLoc) {
  ProgramStateRef state = StateMgr.getInitialState(InitLoc);
  const Decl *D = InitLoc->getDecl();

  // Preconditions.
  // FIXME: It would be nice if we had a more general mechanism to add
  // such preconditions.  Some day.
  do {

    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      // Precondition: the first argument of 'main' is an integer guaranteed
      //  to be > 0.
      const IdentifierInfo *II = FD->getIdentifier();
      if (!II || !(II->getName() == "main" && FD->getNumParams() > 0))
        break;

      const ParmVarDecl *PD = FD->getParamDecl(0);
      QualType T = PD->getType();
      if (!T->isIntegerType())
        break;

      const MemRegion *R = state->getRegion(PD, InitLoc);
      if (!R)
        break;

      SVal V = state->getSVal(loc::MemRegionVal(R));
      SVal Constraint_untested = evalBinOp(state, BO_GT, V,
                                           svalBuilder.makeZeroVal(T),
                                           getContext().IntTy);

      DefinedOrUnknownSVal *Constraint =
        dyn_cast<DefinedOrUnknownSVal>(&Constraint_untested);

      if (!Constraint)
        break;

      if (ProgramStateRef newState = state->assume(*Constraint, true))
        state = newState;
    }
    break;
  }
  while (0);

  if (const ObjCMethodDecl *MD = dyn_cast<ObjCMethodDecl>(D)) {
    // Precondition: 'self' is always non-null upon entry to an Objective-C
    // method.
    const ImplicitParamDecl *SelfD = MD->getSelfDecl();
    const MemRegion *R = state->getRegion(SelfD, InitLoc);
    SVal V = state->getSVal(loc::MemRegionVal(R));

    if (const Loc *LV = dyn_cast<Loc>(&V)) {
      // Assume that the pointer value in 'self' is non-null.
      state = state->assume(*LV, true);
      assert(state && "'self' cannot be null");
    }
  }

  if (const CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(D)) {
    if (!MD->isStatic()) {
      // Precondition: 'this' is always non-null upon entry to the
      // top-level function.  This is our starting assumption for
      // analyzing an "open" program.
      const StackFrameContext *SFC = InitLoc->getCurrentStackFrame();
      if (SFC->getParent() == 0) {
        loc::MemRegionVal L = svalBuilder.getCXXThis(MD, SFC);
        SVal V = state->getSVal(L);
        if (const Loc *LV = dyn_cast<Loc>(&V)) {
          state = state->assume(*LV, true);
          assert(state && "'this' cannot be null");
        }
      }
    }
  }
    
  return state;
}

/// If the value of the given expression is a NonLoc, copy it into a new
/// temporary region, and replace the value of the expression with that.
static ProgramStateRef createTemporaryRegionIfNeeded(ProgramStateRef State,
                                                     const LocationContext *LC,
                                                     const Expr *E) {
  SVal V = State->getSVal(E, LC);

  if (isa<NonLoc>(V)) {
    MemRegionManager &MRMgr = State->getStateManager().getRegionManager();
    const MemRegion *R  = MRMgr.getCXXTempObjectRegion(E, LC);
    State = State->bindLoc(loc::MemRegionVal(R), V);
    State = State->BindExpr(E, LC, loc::MemRegionVal(R));
  }

  return State;
}

//===----------------------------------------------------------------------===//
// Top-level transfer function logic (Dispatcher).
//===----------------------------------------------------------------------===//

/// evalAssume - Called by ConstraintManager. Used to call checker-specific
///  logic for handling assumptions on symbolic values.
ProgramStateRef ExprEngine::processAssume(ProgramStateRef state,
                                              SVal cond, bool assumption) {
  return getCheckerManager().runCheckersForEvalAssume(state, cond, assumption);
}

bool ExprEngine::wantsRegionChangeUpdate(ProgramStateRef state) {
  return getCheckerManager().wantsRegionChangeUpdate(state);
}

ProgramStateRef 
ExprEngine::processRegionChanges(ProgramStateRef state,
                            const StoreManager::InvalidatedSymbols *invalidated,
                                 ArrayRef<const MemRegion *> Explicits,
                                 ArrayRef<const MemRegion *> Regions,
                                 const CallEvent *Call) {
  return getCheckerManager().runCheckersForRegionChanges(state, invalidated,
                                                      Explicits, Regions, Call);
}

void ExprEngine::printState(raw_ostream &Out, ProgramStateRef State,
                            const char *NL, const char *Sep) {
  getCheckerManager().runCheckersForPrintState(Out, State, NL, Sep);
}

void ExprEngine::processEndWorklist(bool hasWorkRemaining) {
  getCheckerManager().runCheckersForEndAnalysis(G, BR, *this);
}

void ExprEngine::processCFGElement(const CFGElement E, ExplodedNode *Pred,
                                   unsigned StmtIdx, NodeBuilderContext *Ctx) {
  currStmtIdx = StmtIdx;
  currBldrCtx = Ctx;

  switch (E.getKind()) {
    case CFGElement::Invalid:
      llvm_unreachable("Unexpected CFGElement kind.");
    case CFGElement::Statement:
      ProcessStmt(const_cast<Stmt*>(E.getAs<CFGStmt>()->getStmt()), Pred);
      return;
    case CFGElement::Initializer:
      ProcessInitializer(E.getAs<CFGInitializer>()->getInitializer(), Pred);
      return;
    case CFGElement::AutomaticObjectDtor:
    case CFGElement::BaseDtor:
    case CFGElement::MemberDtor:
    case CFGElement::TemporaryDtor:
      ProcessImplicitDtor(*E.getAs<CFGImplicitDtor>(), Pred);
      return;
  }
  currBldrCtx = 0;
}

static bool shouldRemoveDeadBindings(AnalysisManager &AMgr,
                                     const CFGStmt S,
                                     const ExplodedNode *Pred,
                                     const LocationContext *LC) {
  
  // Are we never purging state values?
  if (AMgr.options.AnalysisPurgeOpt == PurgeNone)
    return false;

  // Is this the beginning of a basic block?
  if (isa<BlockEntrance>(Pred->getLocation()))
    return true;

  // Is this on a non-expression?
  if (!isa<Expr>(S.getStmt()))
    return true;
    
  // Run before processing a call.
  if (CallEvent::isCallStmt(S.getStmt()))
    return true;

  // Is this an expression that is consumed by another expression?  If so,
  // postpone cleaning out the state.
  ParentMap &PM = LC->getAnalysisDeclContext()->getParentMap();
  return !PM.isConsumedExpr(cast<Expr>(S.getStmt()));
}

void ExprEngine::removeDead(ExplodedNode *Pred, ExplodedNodeSet &Out,
                            const Stmt *ReferenceStmt,
                            const StackFrameContext *LC,
                            const Stmt *DiagnosticStmt,
                            ProgramPoint::Kind K) {
  assert((K == ProgramPoint::PreStmtPurgeDeadSymbolsKind ||
          ReferenceStmt == 0)
          && "PostStmt is not generally supported by the SymbolReaper yet");
  NumRemoveDeadBindings++;
  CleanedState = Pred->getState();
  SymbolReaper SymReaper(LC, ReferenceStmt, SymMgr, getStoreManager());

  getCheckerManager().runCheckersForLiveSymbols(CleanedState, SymReaper);

  // Create a state in which dead bindings are removed from the environment
  // and the store. TODO: The function should just return new env and store,
  // not a new state.
  const StackFrameContext *SFC = LC->getCurrentStackFrame();
  CleanedState = StateMgr.removeDeadBindings(CleanedState, SFC, SymReaper);

  // Process any special transfer function for dead symbols.
  // A tag to track convenience transitions, which can be removed at cleanup.
  static SimpleProgramPointTag cleanupTag("ExprEngine : Clean Node");
  if (!SymReaper.hasDeadSymbols()) {
    // Generate a CleanedNode that has the environment and store cleaned
    // up. Since no symbols are dead, we can optimize and not clean out
    // the constraint manager.
    StmtNodeBuilder Bldr(Pred, Out, *currBldrCtx);
    Bldr.generateNode(DiagnosticStmt, Pred, CleanedState, &cleanupTag, K);

  } else {
    // Call checkers with the non-cleaned state so that they could query the
    // values of the soon to be dead symbols.
    ExplodedNodeSet CheckedSet;
    getCheckerManager().runCheckersForDeadSymbols(CheckedSet, Pred, SymReaper,
                                                  DiagnosticStmt, *this, K);

    // For each node in CheckedSet, generate CleanedNodes that have the
    // environment, the store, and the constraints cleaned up but have the
    // user-supplied states as the predecessors.
    StmtNodeBuilder Bldr(CheckedSet, Out, *currBldrCtx);
    for (ExplodedNodeSet::const_iterator
          I = CheckedSet.begin(), E = CheckedSet.end(); I != E; ++I) {
      ProgramStateRef CheckerState = (*I)->getState();

      // The constraint manager has not been cleaned up yet, so clean up now.
      CheckerState = getConstraintManager().removeDeadBindings(CheckerState,
                                                               SymReaper);

      assert(StateMgr.haveEqualEnvironments(CheckerState, Pred->getState()) &&
        "Checkers are not allowed to modify the Environment as a part of "
        "checkDeadSymbols processing.");
      assert(StateMgr.haveEqualStores(CheckerState, Pred->getState()) &&
        "Checkers are not allowed to modify the Store as a part of "
        "checkDeadSymbols processing.");

      // Create a state based on CleanedState with CheckerState GDM and
      // generate a transition to that state.
      ProgramStateRef CleanedCheckerSt =
        StateMgr.getPersistentStateWithGDM(CleanedState, CheckerState);
      Bldr.generateNode(DiagnosticStmt, *I, CleanedCheckerSt, &cleanupTag, K);
    }
  }
}

void ExprEngine::ProcessStmt(const CFGStmt S,
                             ExplodedNode *Pred) {
  // Reclaim any unnecessary nodes in the ExplodedGraph.
  G.reclaimRecentlyAllocatedNodes();

  currStmt = S.getStmt();
  PrettyStackTraceLoc CrashInfo(getContext().getSourceManager(),
                                currStmt->getLocStart(),
                                "Error evaluating statement");

  // Remove dead bindings and symbols.
  EntryNode = Pred;
  ExplodedNodeSet CleanedStates;
  if (shouldRemoveDeadBindings(AMgr, S, Pred, EntryNode->getLocationContext())){
    removeDead(EntryNode, CleanedStates, currStmt,
               Pred->getStackFrame(), currStmt);
  } else
    CleanedStates.Add(EntryNode);

  // Visit the statement.
  ExplodedNodeSet Dst;
  for (ExplodedNodeSet::iterator I = CleanedStates.begin(),
                                 E = CleanedStates.end(); I != E; ++I) {
    ExplodedNodeSet DstI;
    // Visit the statement.
    Visit(currStmt, *I, DstI);
    Dst.insert(DstI);
  }

  // Enqueue the new nodes onto the work list.
  Engine.enqueue(Dst, currBldrCtx->getBlock(), currStmtIdx);

  // NULL out these variables to cleanup.
  CleanedState = NULL;
  EntryNode = NULL;
  currStmt = 0;
}

void ExprEngine::ProcessInitializer(const CFGInitializer Init,
                                    ExplodedNode *Pred) {
  const CXXCtorInitializer *BMI = Init.getInitializer();

  PrettyStackTraceLoc CrashInfo(getContext().getSourceManager(),
                                BMI->getSourceLocation(),
                                "Error evaluating initializer");

  // We don't set EntryNode and currStmt. And we don't clean up state.
  const StackFrameContext *stackFrame =
                           cast<StackFrameContext>(Pred->getLocationContext());
  const CXXConstructorDecl *decl =
                           cast<CXXConstructorDecl>(stackFrame->getDecl());

  ProgramStateRef State = Pred->getState();
  SVal thisVal = State->getSVal(svalBuilder.getCXXThis(decl, stackFrame));

  PostInitializer PP(BMI, stackFrame);
  ExplodedNodeSet Tmp(Pred);

  // Evaluate the initializer, if necessary
  if (BMI->isAnyMemberInitializer()) {
    // Constructors build the object directly in the field,
    // but non-objects must be copied in from the initializer.
    const Expr *Init = BMI->getInit();
    if (!isa<CXXConstructExpr>(Init)) {
      SVal FieldLoc;
      if (BMI->isIndirectMemberInitializer())
        FieldLoc = State->getLValue(BMI->getIndirectMember(), thisVal);
      else
        FieldLoc = State->getLValue(BMI->getMember(), thisVal);

      SVal InitVal = State->getSVal(BMI->getInit(), stackFrame);

      Tmp.clear();
      evalBind(Tmp, Init, Pred, FieldLoc, InitVal, /*isInit=*/true, &PP);
    }
  } else {
    assert(BMI->isBaseInitializer() || BMI->isDelegatingInitializer());
    // We already did all the work when visiting the CXXConstructExpr.
  }

  // Construct PostInitializer nodes whether the state changed or not,
  // so that the diagnostics don't get confused.
  ExplodedNodeSet Dst;
  NodeBuilder Bldr(Tmp, Dst, *currBldrCtx);
  for (ExplodedNodeSet::iterator I = Tmp.begin(), E = Tmp.end(); I != E; ++I) {
    ExplodedNode *N = *I;
    Bldr.generateNode(PP, N->getState(), N);
  }

  // Enqueue the new nodes onto the work list.
  Engine.enqueue(Dst, currBldrCtx->getBlock(), currStmtIdx);
}

void ExprEngine::ProcessImplicitDtor(const CFGImplicitDtor D,
                                     ExplodedNode *Pred) {
  ExplodedNodeSet Dst;
  switch (D.getKind()) {
  case CFGElement::AutomaticObjectDtor:
    ProcessAutomaticObjDtor(cast<CFGAutomaticObjDtor>(D), Pred, Dst);
    break;
  case CFGElement::BaseDtor:
    ProcessBaseDtor(cast<CFGBaseDtor>(D), Pred, Dst);
    break;
  case CFGElement::MemberDtor:
    ProcessMemberDtor(cast<CFGMemberDtor>(D), Pred, Dst);
    break;
  case CFGElement::TemporaryDtor:
    ProcessTemporaryDtor(cast<CFGTemporaryDtor>(D), Pred, Dst);
    break;
  default:
    llvm_unreachable("Unexpected dtor kind.");
  }

  // Enqueue the new nodes onto the work list.
  Engine.enqueue(Dst, currBldrCtx->getBlock(), currStmtIdx);
}

void ExprEngine::ProcessAutomaticObjDtor(const CFGAutomaticObjDtor Dtor,
                                         ExplodedNode *Pred,
                                         ExplodedNodeSet &Dst) {
  ProgramStateRef state = Pred->getState();
  const VarDecl *varDecl = Dtor.getVarDecl();

  QualType varType = varDecl->getType();

  if (const ReferenceType *refType = varType->getAs<ReferenceType>())
    varType = refType->getPointeeType();

  Loc dest = state->getLValue(varDecl, Pred->getLocationContext());

  VisitCXXDestructor(varType, cast<loc::MemRegionVal>(dest).getRegion(),
                     Dtor.getTriggerStmt(), /*IsBase=*/false, Pred, Dst);
}

void ExprEngine::ProcessBaseDtor(const CFGBaseDtor D,
                                 ExplodedNode *Pred, ExplodedNodeSet &Dst) {
  const LocationContext *LCtx = Pred->getLocationContext();
  ProgramStateRef State = Pred->getState();

  const CXXDestructorDecl *CurDtor = cast<CXXDestructorDecl>(LCtx->getDecl());
  Loc ThisPtr = getSValBuilder().getCXXThis(CurDtor,
                                            LCtx->getCurrentStackFrame());
  SVal ThisVal = Pred->getState()->getSVal(ThisPtr);

  // Create the base object region.
  QualType BaseTy = D.getBaseSpecifier()->getType();
  SVal BaseVal = getStoreManager().evalDerivedToBase(ThisVal, BaseTy);

  VisitCXXDestructor(BaseTy, cast<loc::MemRegionVal>(BaseVal).getRegion(),
                     CurDtor->getBody(), /*IsBase=*/true, Pred, Dst);
}

void ExprEngine::ProcessMemberDtor(const CFGMemberDtor D,
                                   ExplodedNode *Pred, ExplodedNodeSet &Dst) {
  const FieldDecl *Member = D.getFieldDecl();
  ProgramStateRef State = Pred->getState();
  const LocationContext *LCtx = Pred->getLocationContext();

  const CXXDestructorDecl *CurDtor = cast<CXXDestructorDecl>(LCtx->getDecl());
  Loc ThisVal = getSValBuilder().getCXXThis(CurDtor,
                                            LCtx->getCurrentStackFrame());
  SVal FieldVal = State->getLValue(Member, cast<Loc>(State->getSVal(ThisVal)));

  VisitCXXDestructor(Member->getType(),
                     cast<loc::MemRegionVal>(FieldVal).getRegion(),
                     CurDtor->getBody(), /*IsBase=*/false, Pred, Dst);
}

void ExprEngine::ProcessTemporaryDtor(const CFGTemporaryDtor D,
                                      ExplodedNode *Pred,
                                      ExplodedNodeSet &Dst) {}

void ExprEngine::Visit(const Stmt *S, ExplodedNode *Pred,
                       ExplodedNodeSet &DstTop) {
  PrettyStackTraceLoc CrashInfo(getContext().getSourceManager(),
                                S->getLocStart(),
                                "Error evaluating statement");
  ExplodedNodeSet Dst;
  StmtNodeBuilder Bldr(Pred, DstTop, *currBldrCtx);

  // Expressions to ignore.
  if (const Expr *Ex = dyn_cast<Expr>(S))
    S = Ex->IgnoreParens();
  
  // FIXME: add metadata to the CFG so that we can disable
  //  this check when we KNOW that there is no block-level subexpression.
  //  The motivation is that this check requires a hashtable lookup.

  if (S != currStmt && Pred->getLocationContext()->getCFG()->isBlkExpr(S))
    return;

  switch (S->getStmtClass()) {
    // C++ and ARC stuff we don't support yet.
    case Expr::ObjCIndirectCopyRestoreExprClass:
    case Stmt::CXXDependentScopeMemberExprClass:
    case Stmt::CXXPseudoDestructorExprClass:
    case Stmt::CXXTryStmtClass:
    case Stmt::CXXTypeidExprClass:
    case Stmt::CXXUuidofExprClass:
    case Stmt::CXXUnresolvedConstructExprClass:
    case Stmt::DependentScopeDeclRefExprClass:
    case Stmt::UnaryTypeTraitExprClass:
    case Stmt::BinaryTypeTraitExprClass:
    case Stmt::TypeTraitExprClass:
    case Stmt::ArrayTypeTraitExprClass:
    case Stmt::ExpressionTraitExprClass:
    case Stmt::UnresolvedLookupExprClass:
    case Stmt::UnresolvedMemberExprClass:
    case Stmt::CXXNoexceptExprClass:
    case Stmt::PackExpansionExprClass:
    case Stmt::SubstNonTypeTemplateParmPackExprClass:
    case Stmt::FunctionParmPackExprClass:
    case Stmt::SEHTryStmtClass:
    case Stmt::SEHExceptStmtClass:
    case Stmt::LambdaExprClass:
    case Stmt::SEHFinallyStmtClass: {
      const ExplodedNode *node = Bldr.generateSink(S, Pred, Pred->getState());
      Engine.addAbortedBlock(node, currBldrCtx->getBlock());
      break;
    }
    
    case Stmt::ParenExprClass:
      llvm_unreachable("ParenExprs already handled.");
    case Stmt::GenericSelectionExprClass:
      llvm_unreachable("GenericSelectionExprs already handled.");
    // Cases that should never be evaluated simply because they shouldn't
    // appear in the CFG.
    case Stmt::BreakStmtClass:
    case Stmt::CaseStmtClass:
    case Stmt::CompoundStmtClass:
    case Stmt::ContinueStmtClass:
    case Stmt::CXXForRangeStmtClass:
    case Stmt::DefaultStmtClass:
    case Stmt::DoStmtClass:
    case Stmt::ForStmtClass:
    case Stmt::GotoStmtClass:
    case Stmt::IfStmtClass:
    case Stmt::IndirectGotoStmtClass:
    case Stmt::LabelStmtClass:
    case Stmt::AttributedStmtClass:
    case Stmt::NoStmtClass:
    case Stmt::NullStmtClass:
    case Stmt::SwitchStmtClass:
    case Stmt::WhileStmtClass:
    case Expr::MSDependentExistsStmtClass:
      llvm_unreachable("Stmt should not be in analyzer evaluation loop");

    case Stmt::ObjCSubscriptRefExprClass:
    case Stmt::ObjCPropertyRefExprClass:
      llvm_unreachable("These are handled by PseudoObjectExpr");

    case Stmt::GNUNullExprClass: {
      // GNU __null is a pointer-width integer, not an actual pointer.
      ProgramStateRef state = Pred->getState();
      state = state->BindExpr(S, Pred->getLocationContext(),
                              svalBuilder.makeIntValWithPtrWidth(0, false));
      Bldr.generateNode(S, Pred, state);
      break;
    }

    case Stmt::ObjCAtSynchronizedStmtClass:
      Bldr.takeNodes(Pred);
      VisitObjCAtSynchronizedStmt(cast<ObjCAtSynchronizedStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::ExprWithCleanupsClass:
      // Handled due to fully linearised CFG.
      break;

    // Cases not handled yet; but will handle some day.
    case Stmt::DesignatedInitExprClass:
    case Stmt::ExtVectorElementExprClass:
    case Stmt::ImaginaryLiteralClass:
    case Stmt::ObjCAtCatchStmtClass:
    case Stmt::ObjCAtFinallyStmtClass:
    case Stmt::ObjCAtTryStmtClass:
    case Stmt::ObjCAutoreleasePoolStmtClass:
    case Stmt::ObjCEncodeExprClass:
    case Stmt::ObjCIsaExprClass:
    case Stmt::ObjCProtocolExprClass:
    case Stmt::ObjCSelectorExprClass:
    case Stmt::ParenListExprClass:
    case Stmt::PredefinedExprClass:
    case Stmt::ShuffleVectorExprClass:
    case Stmt::VAArgExprClass:
    case Stmt::CUDAKernelCallExprClass:
    case Stmt::OpaqueValueExprClass:
    case Stmt::AsTypeExprClass:
    case Stmt::AtomicExprClass:
      // Fall through.

    // Cases we intentionally don't evaluate, since they don't need
    // to be explicitly evaluated.
    case Stmt::AddrLabelExprClass:
    case Stmt::IntegerLiteralClass:
    case Stmt::CharacterLiteralClass:
    case Stmt::ImplicitValueInitExprClass:
    case Stmt::CXXScalarValueInitExprClass:
    case Stmt::CXXBoolLiteralExprClass:
    case Stmt::ObjCBoolLiteralExprClass:
    case Stmt::FloatingLiteralClass:
    case Stmt::SizeOfPackExprClass:
    case Stmt::StringLiteralClass:
    case Stmt::ObjCStringLiteralClass:
    case Stmt::CXXBindTemporaryExprClass:
    case Stmt::CXXDefaultArgExprClass:
    case Stmt::SubstNonTypeTemplateParmExprClass:
    case Stmt::CXXNullPtrLiteralExprClass: {
      Bldr.takeNodes(Pred);
      ExplodedNodeSet preVisit;
      getCheckerManager().runCheckersForPreStmt(preVisit, Pred, S, *this);
      getCheckerManager().runCheckersForPostStmt(Dst, preVisit, S, *this);
      Bldr.addNodes(Dst);
      break;
    }

    case Expr::ObjCArrayLiteralClass:
    case Expr::ObjCDictionaryLiteralClass:
      // FIXME: explicitly model with a region and the actual contents
      // of the container.  For now, conjure a symbol.
    case Expr::ObjCBoxedExprClass: {
      Bldr.takeNodes(Pred);

      ExplodedNodeSet preVisit;
      getCheckerManager().runCheckersForPreStmt(preVisit, Pred, S, *this);
      
      ExplodedNodeSet Tmp;
      StmtNodeBuilder Bldr2(preVisit, Tmp, *currBldrCtx);

      const Expr *Ex = cast<Expr>(S);
      QualType resultType = Ex->getType();

      for (ExplodedNodeSet::iterator it = preVisit.begin(), et = preVisit.end();
           it != et; ++it) {      
        ExplodedNode *N = *it;
        const LocationContext *LCtx = N->getLocationContext();
        SVal result = svalBuilder.conjureSymbolVal(0, Ex, LCtx, resultType,
                                                   currBldrCtx->blockCount());
        ProgramStateRef state = N->getState()->BindExpr(Ex, LCtx, result);
        Bldr2.generateNode(S, N, state);
      }
      
      getCheckerManager().runCheckersForPostStmt(Dst, Tmp, S, *this);
      Bldr.addNodes(Dst);
      break;      
    }

    case Stmt::ArraySubscriptExprClass:
      Bldr.takeNodes(Pred);
      VisitLvalArraySubscriptExpr(cast<ArraySubscriptExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::GCCAsmStmtClass:
      Bldr.takeNodes(Pred);
      VisitGCCAsmStmt(cast<GCCAsmStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::MSAsmStmtClass:
      Bldr.takeNodes(Pred);
      VisitMSAsmStmt(cast<MSAsmStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::BlockExprClass:
      Bldr.takeNodes(Pred);
      VisitBlockExpr(cast<BlockExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::BinaryOperatorClass: {
      const BinaryOperator* B = cast<BinaryOperator>(S);
      if (B->isLogicalOp()) {
        Bldr.takeNodes(Pred);
        VisitLogicalExpr(B, Pred, Dst);
        Bldr.addNodes(Dst);
        break;
      }
      else if (B->getOpcode() == BO_Comma) {
        ProgramStateRef state = Pred->getState();
        Bldr.generateNode(B, Pred,
                          state->BindExpr(B, Pred->getLocationContext(),
                                          state->getSVal(B->getRHS(),
                                                  Pred->getLocationContext())));
        break;
      }

      Bldr.takeNodes(Pred);
      
      if (AMgr.options.eagerlyAssumeBinOpBifurcation &&
          (B->isRelationalOp() || B->isEqualityOp())) {
        ExplodedNodeSet Tmp;
        VisitBinaryOperator(cast<BinaryOperator>(S), Pred, Tmp);
        evalEagerlyAssumeBinOpBifurcation(Dst, Tmp, cast<Expr>(S));
      }
      else
        VisitBinaryOperator(cast<BinaryOperator>(S), Pred, Dst);

      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::CXXOperatorCallExprClass: {
      const CXXOperatorCallExpr *OCE = cast<CXXOperatorCallExpr>(S);

      // For instance method operators, make sure the 'this' argument has a
      // valid region.
      const Decl *Callee = OCE->getCalleeDecl();
      if (const CXXMethodDecl *MD = dyn_cast_or_null<CXXMethodDecl>(Callee)) {
        if (MD->isInstance()) {
          ProgramStateRef State = Pred->getState();
          const LocationContext *LCtx = Pred->getLocationContext();
          ProgramStateRef NewState =
            createTemporaryRegionIfNeeded(State, LCtx, OCE->getArg(0));
          if (NewState != State)
            Pred = Bldr.generateNode(OCE, Pred, NewState, /*Tag=*/0,
                                     ProgramPoint::PreStmtKind);
        }
      }
      // FALLTHROUGH
    }
    case Stmt::CallExprClass:
    case Stmt::CXXMemberCallExprClass:
    case Stmt::UserDefinedLiteralClass: {
      Bldr.takeNodes(Pred);
      VisitCallExpr(cast<CallExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }
    
    case Stmt::CXXCatchStmtClass: {
      Bldr.takeNodes(Pred);
      VisitCXXCatchStmt(cast<CXXCatchStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::CXXTemporaryObjectExprClass:
    case Stmt::CXXConstructExprClass: {      
      Bldr.takeNodes(Pred);
      VisitCXXConstructExpr(cast<CXXConstructExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::CXXNewExprClass: {
      Bldr.takeNodes(Pred);
      const CXXNewExpr *NE = cast<CXXNewExpr>(S);
      VisitCXXNewExpr(NE, Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::CXXDeleteExprClass: {
      Bldr.takeNodes(Pred);
      const CXXDeleteExpr *CDE = cast<CXXDeleteExpr>(S);
      VisitCXXDeleteExpr(CDE, Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }
      // FIXME: ChooseExpr is really a constant.  We need to fix
      //        the CFG do not model them as explicit control-flow.

    case Stmt::ChooseExprClass: { // __builtin_choose_expr
      Bldr.takeNodes(Pred);
      const ChooseExpr *C = cast<ChooseExpr>(S);
      VisitGuardedExpr(C, C->getLHS(), C->getRHS(), Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::CompoundAssignOperatorClass:
      Bldr.takeNodes(Pred);
      VisitBinaryOperator(cast<BinaryOperator>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::CompoundLiteralExprClass:
      Bldr.takeNodes(Pred);
      VisitCompoundLiteralExpr(cast<CompoundLiteralExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::BinaryConditionalOperatorClass:
    case Stmt::ConditionalOperatorClass: { // '?' operator
      Bldr.takeNodes(Pred);
      const AbstractConditionalOperator *C
        = cast<AbstractConditionalOperator>(S);
      VisitGuardedExpr(C, C->getTrueExpr(), C->getFalseExpr(), Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::CXXThisExprClass:
      Bldr.takeNodes(Pred);
      VisitCXXThisExpr(cast<CXXThisExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::DeclRefExprClass: {
      Bldr.takeNodes(Pred);
      const DeclRefExpr *DE = cast<DeclRefExpr>(S);
      VisitCommonDeclRefExpr(DE, DE->getDecl(), Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::DeclStmtClass:
      Bldr.takeNodes(Pred);
      VisitDeclStmt(cast<DeclStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::ImplicitCastExprClass:
    case Stmt::CStyleCastExprClass:
    case Stmt::CXXStaticCastExprClass:
    case Stmt::CXXDynamicCastExprClass:
    case Stmt::CXXReinterpretCastExprClass:
    case Stmt::CXXConstCastExprClass:
    case Stmt::CXXFunctionalCastExprClass: 
    case Stmt::ObjCBridgedCastExprClass: {
      Bldr.takeNodes(Pred);
      const CastExpr *C = cast<CastExpr>(S);
      // Handle the previsit checks.
      ExplodedNodeSet dstPrevisit;
      getCheckerManager().runCheckersForPreStmt(dstPrevisit, Pred, C, *this);
      
      // Handle the expression itself.
      ExplodedNodeSet dstExpr;
      for (ExplodedNodeSet::iterator i = dstPrevisit.begin(),
                                     e = dstPrevisit.end(); i != e ; ++i) { 
        VisitCast(C, C->getSubExpr(), *i, dstExpr);
      }

      // Handle the postvisit checks.
      getCheckerManager().runCheckersForPostStmt(Dst, dstExpr, C, *this);
      Bldr.addNodes(Dst);
      break;
    }

    case Expr::MaterializeTemporaryExprClass: {
      Bldr.takeNodes(Pred);
      const MaterializeTemporaryExpr *MTE = cast<MaterializeTemporaryExpr>(S);
      CreateCXXTemporaryObject(MTE, Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }
      
    case Stmt::InitListExprClass:
      Bldr.takeNodes(Pred);
      VisitInitListExpr(cast<InitListExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::MemberExprClass:
      Bldr.takeNodes(Pred);
      VisitMemberExpr(cast<MemberExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::ObjCIvarRefExprClass:
      Bldr.takeNodes(Pred);
      VisitLvalObjCIvarRefExpr(cast<ObjCIvarRefExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::ObjCForCollectionStmtClass:
      Bldr.takeNodes(Pred);
      VisitObjCForCollectionStmt(cast<ObjCForCollectionStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::ObjCMessageExprClass:
      Bldr.takeNodes(Pred);
      VisitObjCMessage(cast<ObjCMessageExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::ObjCAtThrowStmtClass:
    case Stmt::CXXThrowExprClass:
      // FIXME: This is not complete.  We basically treat @throw as
      // an abort.
      Bldr.generateSink(S, Pred, Pred->getState());
      break;

    case Stmt::ReturnStmtClass:
      Bldr.takeNodes(Pred);
      VisitReturnStmt(cast<ReturnStmt>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::OffsetOfExprClass:
      Bldr.takeNodes(Pred);
      VisitOffsetOfExpr(cast<OffsetOfExpr>(S), Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::UnaryExprOrTypeTraitExprClass:
      Bldr.takeNodes(Pred);
      VisitUnaryExprOrTypeTraitExpr(cast<UnaryExprOrTypeTraitExpr>(S),
                                    Pred, Dst);
      Bldr.addNodes(Dst);
      break;

    case Stmt::StmtExprClass: {
      const StmtExpr *SE = cast<StmtExpr>(S);

      if (SE->getSubStmt()->body_empty()) {
        // Empty statement expression.
        assert(SE->getType() == getContext().VoidTy
               && "Empty statement expression must have void type.");
        break;
      }

      if (Expr *LastExpr = dyn_cast<Expr>(*SE->getSubStmt()->body_rbegin())) {
        ProgramStateRef state = Pred->getState();
        Bldr.generateNode(SE, Pred,
                          state->BindExpr(SE, Pred->getLocationContext(),
                                          state->getSVal(LastExpr,
                                                  Pred->getLocationContext())));
      }
      break;
    }

    case Stmt::UnaryOperatorClass: {
      Bldr.takeNodes(Pred);
      const UnaryOperator *U = cast<UnaryOperator>(S);
      if (AMgr.options.eagerlyAssumeBinOpBifurcation && (U->getOpcode() == UO_LNot)) {
        ExplodedNodeSet Tmp;
        VisitUnaryOperator(U, Pred, Tmp);
        evalEagerlyAssumeBinOpBifurcation(Dst, Tmp, U);
      }
      else
        VisitUnaryOperator(U, Pred, Dst);
      Bldr.addNodes(Dst);
      break;
    }

    case Stmt::PseudoObjectExprClass: {
      Bldr.takeNodes(Pred);
      ProgramStateRef state = Pred->getState();
      const PseudoObjectExpr *PE = cast<PseudoObjectExpr>(S);
      if (const Expr *Result = PE->getResultExpr()) { 
        SVal V = state->getSVal(Result, Pred->getLocationContext());
        Bldr.generateNode(S, Pred,
                          state->BindExpr(S, Pred->getLocationContext(), V));
      }
      else
        Bldr.generateNode(S, Pred,
                          state->BindExpr(S, Pred->getLocationContext(),
                                                   UnknownVal()));

      Bldr.addNodes(Dst);
      break;
    }
  }
}

bool ExprEngine::replayWithoutInlining(ExplodedNode *N,
                                       const LocationContext *CalleeLC) {
  const StackFrameContext *CalleeSF = CalleeLC->getCurrentStackFrame();
  const StackFrameContext *CallerSF = CalleeSF->getParent()->getCurrentStackFrame();
  assert(CalleeSF && CallerSF);
  ExplodedNode *BeforeProcessingCall = 0;
  const Stmt *CE = CalleeSF->getCallSite();

  // Find the first node before we started processing the call expression.
  while (N) {
    ProgramPoint L = N->getLocation();
    BeforeProcessingCall = N;
    N = N->pred_empty() ? NULL : *(N->pred_begin());

    // Skip the nodes corresponding to the inlined code.
    if (L.getLocationContext()->getCurrentStackFrame() != CallerSF)
      continue;
    // We reached the caller. Find the node right before we started
    // processing the call.
    if (L.isPurgeKind())
      continue;
    if (isa<PreImplicitCall>(&L))
      continue;
    if (isa<CallEnter>(&L))
      continue;
    if (const StmtPoint *SP = dyn_cast<StmtPoint>(&L))
      if (SP->getStmt() == CE)
        continue;
    break;
  }

  if (!BeforeProcessingCall)
    return false;

  // TODO: Clean up the unneeded nodes.

  // Build an Epsilon node from which we will restart the analyzes.
  // Note that CE is permitted to be NULL!
  ProgramPoint NewNodeLoc =
               EpsilonPoint(BeforeProcessingCall->getLocationContext(), CE);
  // Add the special flag to GDM to signal retrying with no inlining.
  // Note, changing the state ensures that we are not going to cache out.
  ProgramStateRef NewNodeState = BeforeProcessingCall->getState();
  NewNodeState = NewNodeState->set<ReplayWithoutInlining>((void*)CE);

  // Make the new node a successor of BeforeProcessingCall.
  bool IsNew = false;
  ExplodedNode *NewNode = G.getNode(NewNodeLoc, NewNodeState, false, &IsNew);
  // We cached out at this point. Caching out is common due to us backtracking
  // from the inlined function, which might spawn several paths.
  if (!IsNew)
    return true;

  NewNode->addPredecessor(BeforeProcessingCall, G);

  // Add the new node to the work list.
  Engine.enqueueStmtNode(NewNode, CalleeSF->getCallSiteBlock(),
                                  CalleeSF->getIndex());
  NumTimesRetriedWithoutInlining++;
  return true;
}

/// Block entrance.  (Update counters).
void ExprEngine::processCFGBlockEntrance(const BlockEdge &L,
                                         NodeBuilderWithSinks &nodeBuilder, 
                                         ExplodedNode *Pred) {
  
  // FIXME: Refactor this into a checker.
  if (nodeBuilder.getContext().blockCount() >= AMgr.options.maxBlockVisitOnPath) {
    static SimpleProgramPointTag tag("ExprEngine : Block count exceeded");
    const ExplodedNode *Sink =
                   nodeBuilder.generateSink(Pred->getState(), Pred, &tag);

    // Check if we stopped at the top level function or not.
    // Root node should have the location context of the top most function.
    const LocationContext *CalleeLC = Pred->getLocation().getLocationContext();
    const LocationContext *CalleeSF = CalleeLC->getCurrentStackFrame();
    const LocationContext *RootLC =
                        (*G.roots_begin())->getLocation().getLocationContext();
    if (RootLC->getCurrentStackFrame() != CalleeSF) {
      Engine.FunctionSummaries->markReachedMaxBlockCount(CalleeSF->getDecl());

      // Re-run the call evaluation without inlining it, by storing the
      // no-inlining policy in the state and enqueuing the new work item on
      // the list. Replay should almost never fail. Use the stats to catch it
      // if it does.
      if ((!AMgr.options.NoRetryExhausted &&
           replayWithoutInlining(Pred, CalleeLC)))
        return;
      NumMaxBlockCountReachedInInlined++;
    } else
      NumMaxBlockCountReached++;

    // Make sink nodes as exhausted(for stats) only if retry failed.
    Engine.blocksExhausted.push_back(std::make_pair(L, Sink));
  }
}

//===----------------------------------------------------------------------===//
// Branch processing.
//===----------------------------------------------------------------------===//

/// RecoverCastedSymbol - A helper function for ProcessBranch that is used
/// to try to recover some path-sensitivity for casts of symbolic
/// integers that promote their values (which are currently not tracked well).
/// This function returns the SVal bound to Condition->IgnoreCasts if all the
//  cast(s) did was sign-extend the original value.
static SVal RecoverCastedSymbol(ProgramStateManager& StateMgr,
                                ProgramStateRef state,
                                const Stmt *Condition,
                                const LocationContext *LCtx,
                                ASTContext &Ctx) {

  const Expr *Ex = dyn_cast<Expr>(Condition);
  if (!Ex)
    return UnknownVal();

  uint64_t bits = 0;
  bool bitsInit = false;

  while (const CastExpr *CE = dyn_cast<CastExpr>(Ex)) {
    QualType T = CE->getType();

    if (!T->isIntegerType())
      return UnknownVal();

    uint64_t newBits = Ctx.getTypeSize(T);
    if (!bitsInit || newBits < bits) {
      bitsInit = true;
      bits = newBits;
    }

    Ex = CE->getSubExpr();
  }

  // We reached a non-cast.  Is it a symbolic value?
  QualType T = Ex->getType();

  if (!bitsInit || !T->isIntegerType() || Ctx.getTypeSize(T) > bits)
    return UnknownVal();

  return state->getSVal(Ex, LCtx);
}

static const Stmt *ResolveCondition(const Stmt *Condition,
                                    const CFGBlock *B) {
  if (const Expr *Ex = dyn_cast<Expr>(Condition))
    Condition = Ex->IgnoreParens();

  const BinaryOperator *BO = dyn_cast<BinaryOperator>(Condition);
  if (!BO || !BO->isLogicalOp())
    return Condition;

  // For logical operations, we still have the case where some branches
  // use the traditional "merge" approach and others sink the branch
  // directly into the basic blocks representing the logical operation.
  // We need to distinguish between those two cases here.

  // The invariants are still shifting, but it is possible that the
  // last element in a CFGBlock is not a CFGStmt.  Look for the last
  // CFGStmt as the value of the condition.
  CFGBlock::const_reverse_iterator I = B->rbegin(), E = B->rend();
  for (; I != E; ++I) {
    CFGElement Elem = *I;
    CFGStmt *CS = dyn_cast<CFGStmt>(&Elem);
    if (!CS)
      continue;
    if (CS->getStmt() != Condition)
      break;
    return Condition;
  }

  assert(I != E);

  while (Condition) {
    BO = dyn_cast<BinaryOperator>(Condition);
    if (!BO || !BO->isLogicalOp())
      return Condition;
    Condition = BO->getRHS()->IgnoreParens();
  }
  llvm_unreachable("could not resolve condition");
}

void ExprEngine::processBranch(const Stmt *Condition, const Stmt *Term,
                               NodeBuilderContext& BldCtx,
                               ExplodedNode *Pred,
                               ExplodedNodeSet &Dst,
                               const CFGBlock *DstT,
                               const CFGBlock *DstF) {
  currBldrCtx = &BldCtx;

  // Check for NULL conditions; e.g. "for(;;)"
  if (!Condition) {
    BranchNodeBuilder NullCondBldr(Pred, Dst, BldCtx, DstT, DstF);
    NullCondBldr.markInfeasible(false);
    NullCondBldr.generateNode(Pred->getState(), true, Pred);
    return;
  }


  // Resolve the condition in the precense of nested '||' and '&&'.
  if (const Expr *Ex = dyn_cast<Expr>(Condition))
    Condition = Ex->IgnoreParens();

  Condition = ResolveCondition(Condition, BldCtx.getBlock());
  PrettyStackTraceLoc CrashInfo(getContext().getSourceManager(),
                                Condition->getLocStart(),
                                "Error evaluating branch");

  ExplodedNodeSet CheckersOutSet;
  getCheckerManager().runCheckersForBranchCondition(Condition, CheckersOutSet,
                                                    Pred, *this);
  // We generated only sinks.
  if (CheckersOutSet.empty())
    return;

  BranchNodeBuilder builder(CheckersOutSet, Dst, BldCtx, DstT, DstF);
  for (NodeBuilder::iterator I = CheckersOutSet.begin(),
                             E = CheckersOutSet.end(); E != I; ++I) {
    ExplodedNode *PredI = *I;

    if (PredI->isSink())
      continue;

    ProgramStateRef PrevState = Pred->getState();
    SVal X = PrevState->getSVal(Condition, Pred->getLocationContext());

    if (X.isUnknownOrUndef()) {
      // Give it a chance to recover from unknown.
      if (const Expr *Ex = dyn_cast<Expr>(Condition)) {
        if (Ex->getType()->isIntegerType()) {
          // Try to recover some path-sensitivity.  Right now casts of symbolic
          // integers that promote their values are currently not tracked well.
          // If 'Condition' is such an expression, try and recover the
          // underlying value and use that instead.
          SVal recovered = RecoverCastedSymbol(getStateManager(),
                                               PrevState, Condition,
                                               Pred->getLocationContext(),
                                               getContext());

          if (!recovered.isUnknown()) {
            X = recovered;
          }
        }
      }
    }
    
    // If the condition is still unknown, give up.
    if (X.isUnknownOrUndef()) {
      builder.generateNode(PrevState, true, PredI);
      builder.generateNode(PrevState, false, PredI);
      continue;
    }

    DefinedSVal V = cast<DefinedSVal>(X);

    // Process the true branch.
    if (builder.isFeasible(true)) {
      if (ProgramStateRef state = PrevState->assume(V, true))
        builder.generateNode(state, true, PredI);
      else
        builder.markInfeasible(true);
    }

    // Process the false branch.
    if (builder.isFeasible(false)) {
      if (ProgramStateRef state = PrevState->assume(V, false))
        builder.generateNode(state, false, PredI);
      else
        builder.markInfeasible(false);
    }
  }
  currBldrCtx = 0;
}

/// processIndirectGoto - Called by CoreEngine.  Used to generate successor
///  nodes by processing the 'effects' of a computed goto jump.
void ExprEngine::processIndirectGoto(IndirectGotoNodeBuilder &builder) {

  ProgramStateRef state = builder.getState();
  SVal V = state->getSVal(builder.getTarget(), builder.getLocationContext());

  // Three possibilities:
  //
  //   (1) We know the computed label.
  //   (2) The label is NULL (or some other constant), or Undefined.
  //   (3) We have no clue about the label.  Dispatch to all targets.
  //

  typedef IndirectGotoNodeBuilder::iterator iterator;

  if (isa<loc::GotoLabel>(V)) {
    const LabelDecl *L = cast<loc::GotoLabel>(V).getLabel();

    for (iterator I = builder.begin(), E = builder.end(); I != E; ++I) {
      if (I.getLabel() == L) {
        builder.generateNode(I, state);
        return;
      }
    }

    llvm_unreachable("No block with label.");
  }

  if (isa<loc::ConcreteInt>(V) || isa<UndefinedVal>(V)) {
    // Dispatch to the first target and mark it as a sink.
    //ExplodedNode* N = builder.generateNode(builder.begin(), state, true);
    // FIXME: add checker visit.
    //    UndefBranches.insert(N);
    return;
  }

  // This is really a catch-all.  We don't support symbolics yet.
  // FIXME: Implement dispatch for symbolic pointers.

  for (iterator I=builder.begin(), E=builder.end(); I != E; ++I)
    builder.generateNode(I, state);
}

/// ProcessEndPath - Called by CoreEngine.  Used to generate end-of-path
///  nodes when the control reaches the end of a function.
void ExprEngine::processEndOfFunction(NodeBuilderContext& BC,
                                      ExplodedNode *Pred) {
  StateMgr.EndPath(Pred->getState());

  ExplodedNodeSet Dst;
  if (Pred->getLocationContext()->inTopFrame()) {
    // Remove dead symbols.
    ExplodedNodeSet AfterRemovedDead;
    removeDeadOnEndOfFunction(BC, Pred, AfterRemovedDead);

    // Notify checkers.
    for (ExplodedNodeSet::iterator I = AfterRemovedDead.begin(),
        E = AfterRemovedDead.end(); I != E; ++I) {
      getCheckerManager().runCheckersForEndPath(BC, Dst, *I, *this);
    }
  } else {
    getCheckerManager().runCheckersForEndPath(BC, Dst, Pred, *this);
  }

  Engine.enqueueEndOfFunction(Dst);
}

/// ProcessSwitch - Called by CoreEngine.  Used to generate successor
///  nodes by processing the 'effects' of a switch statement.
void ExprEngine::processSwitch(SwitchNodeBuilder& builder) {
  typedef SwitchNodeBuilder::iterator iterator;
  ProgramStateRef state = builder.getState();
  const Expr *CondE = builder.getCondition();
  SVal  CondV_untested = state->getSVal(CondE, builder.getLocationContext());

  if (CondV_untested.isUndef()) {
    //ExplodedNode* N = builder.generateDefaultCaseNode(state, true);
    // FIXME: add checker
    //UndefBranches.insert(N);

    return;
  }
  DefinedOrUnknownSVal CondV = cast<DefinedOrUnknownSVal>(CondV_untested);

  ProgramStateRef DefaultSt = state;
  
  iterator I = builder.begin(), EI = builder.end();
  bool defaultIsFeasible = I == EI;

  for ( ; I != EI; ++I) {
    // Successor may be pruned out during CFG construction.
    if (!I.getBlock())
      continue;
    
    const CaseStmt *Case = I.getCase();

    // Evaluate the LHS of the case value.
    llvm::APSInt V1 = Case->getLHS()->EvaluateKnownConstInt(getContext());
    assert(V1.getBitWidth() == getContext().getTypeSize(CondE->getType()));

    // Get the RHS of the case, if it exists.
    llvm::APSInt V2;
    if (const Expr *E = Case->getRHS())
      V2 = E->EvaluateKnownConstInt(getContext());
    else
      V2 = V1;

    // FIXME: Eventually we should replace the logic below with a range
    //  comparison, rather than concretize the values within the range.
    //  This should be easy once we have "ranges" for NonLVals.

    do {
      nonloc::ConcreteInt CaseVal(getBasicVals().getValue(V1));
      DefinedOrUnknownSVal Res = svalBuilder.evalEQ(DefaultSt ? DefaultSt : state,
                                               CondV, CaseVal);

      // Now "assume" that the case matches.
      if (ProgramStateRef stateNew = state->assume(Res, true)) {
        builder.generateCaseStmtNode(I, stateNew);

        // If CondV evaluates to a constant, then we know that this
        // is the *only* case that we can take, so stop evaluating the
        // others.
        if (isa<nonloc::ConcreteInt>(CondV))
          return;
      }

      // Now "assume" that the case doesn't match.  Add this state
      // to the default state (if it is feasible).
      if (DefaultSt) {
        if (ProgramStateRef stateNew = DefaultSt->assume(Res, false)) {
          defaultIsFeasible = true;
          DefaultSt = stateNew;
        }
        else {
          defaultIsFeasible = false;
          DefaultSt = NULL;
        }
      }

      // Concretize the next value in the range.
      if (V1 == V2)
        break;

      ++V1;
      assert (V1 <= V2);

    } while (true);
  }

  if (!defaultIsFeasible)
    return;

  // If we have switch(enum value), the default branch is not
  // feasible if all of the enum constants not covered by 'case:' statements
  // are not feasible values for the switch condition.
  //
  // Note that this isn't as accurate as it could be.  Even if there isn't
  // a case for a particular enum value as long as that enum value isn't
  // feasible then it shouldn't be considered for making 'default:' reachable.
  const SwitchStmt *SS = builder.getSwitch();
  const Expr *CondExpr = SS->getCond()->IgnoreParenImpCasts();
  if (CondExpr->getType()->getAs<EnumType>()) {
    if (SS->isAllEnumCasesCovered())
      return;
  }

  builder.generateDefaultCaseNode(DefaultSt);
}

//===----------------------------------------------------------------------===//
// Transfer functions: Loads and stores.
//===----------------------------------------------------------------------===//

void ExprEngine::VisitCommonDeclRefExpr(const Expr *Ex, const NamedDecl *D,
                                        ExplodedNode *Pred,
                                        ExplodedNodeSet &Dst) {
  StmtNodeBuilder Bldr(Pred, Dst, *currBldrCtx);

  ProgramStateRef state = Pred->getState();
  const LocationContext *LCtx = Pred->getLocationContext();

  if (const VarDecl *VD = dyn_cast<VarDecl>(D)) {
    assert(Ex->isGLValue());
    SVal V = state->getLValue(VD, Pred->getLocationContext());

    // For references, the 'lvalue' is the pointer address stored in the
    // reference region.
    if (VD->getType()->isReferenceType()) {
      if (const MemRegion *R = V.getAsRegion())
        V = state->getSVal(R);
      else
        V = UnknownVal();
    }

    Bldr.generateNode(Ex, Pred, state->BindExpr(Ex, LCtx, V), 0,
                      ProgramPoint::PostLValueKind);
    return;
  }
  if (const EnumConstantDecl *ED = dyn_cast<EnumConstantDecl>(D)) {
    assert(!Ex->isGLValue());
    SVal V = svalBuilder.makeIntVal(ED->getInitVal());
    Bldr.generateNode(Ex, Pred, state->BindExpr(Ex, LCtx, V));
    return;
  }
  if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    SVal V = svalBuilder.getFunctionPointer(FD);
    Bldr.generateNode(Ex, Pred, state->BindExpr(Ex, LCtx, V), 0,
                      ProgramPoint::PostLValueKind);
    return;
  }
  if (isa<FieldDecl>(D)) {
    // FIXME: Compute lvalue of field pointers-to-member.
    // Right now we just use a non-null void pointer, so that it gives proper
    // results in boolean contexts.
    SVal V = svalBuilder.conjureSymbolVal(Ex, LCtx, getContext().VoidPtrTy,
                                          currBldrCtx->blockCount());
    state = state->assume(cast<DefinedOrUnknownSVal>(V), true);
    Bldr.generateNode(Ex, Pred, state->BindExpr(Ex, LCtx, V), 0,
		      ProgramPoint::PostLValueKind);
    return;
  }

  llvm_unreachable("Support for this Decl not implemented.");
}

/// VisitArraySubscriptExpr - Transfer function for array accesses
void ExprEngine::VisitLvalArraySubscriptExpr(const ArraySubscriptExpr *A,
                                             ExplodedNode *Pred,
                                             ExplodedNodeSet &Dst){

  const Expr *Base = A->getBase()->IgnoreParens();
  const Expr *Idx  = A->getIdx()->IgnoreParens();
  

  ExplodedNodeSet checkerPreStmt;
  getCheckerManager().runCheckersForPreStmt(checkerPreStmt, Pred, A, *this);

  StmtNodeBuilder Bldr(checkerPreStmt, Dst, *currBldrCtx);

  for (ExplodedNodeSet::iterator it = checkerPreStmt.begin(),
                                 ei = checkerPreStmt.end(); it != ei; ++it) {
    const LocationContext *LCtx = (*it)->getLocationContext();
    ProgramStateRef state = (*it)->getState();
    SVal V = state->getLValue(A->getType(),
                              state->getSVal(Idx, LCtx),
                              state->getSVal(Base, LCtx));
    assert(A->isGLValue());
    Bldr.generateNode(A, *it, state->BindExpr(A, LCtx, V), 0, 
                      ProgramPoint::PostLValueKind);
  }
}

/// VisitMemberExpr - Transfer function for member expressions.
void ExprEngine::VisitMemberExpr(const MemberExpr *M, ExplodedNode *Pred,
                                 ExplodedNodeSet &TopDst) {

  StmtNodeBuilder Bldr(Pred, TopDst, *currBldrCtx);
  ExplodedNodeSet Dst;
  ValueDecl *Member = M->getMemberDecl();

  // Handle static member variables and enum constants accessed via
  // member syntax.
  if (isa<VarDecl>(Member) || isa<EnumConstantDecl>(Member)) {
    Bldr.takeNodes(Pred);
    VisitCommonDeclRefExpr(M, Member, Pred, Dst);
    Bldr.addNodes(Dst);
    return;
  }

  ProgramStateRef state = Pred->getState();
  const LocationContext *LCtx = Pred->getLocationContext();
  Expr *BaseExpr = M->getBase();

  // Handle C++ method calls.
  if (const CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(Member)) {
    if (MD->isInstance())
      state = createTemporaryRegionIfNeeded(state, LCtx, BaseExpr);

    SVal MDVal = svalBuilder.getFunctionPointer(MD);
    state = state->BindExpr(M, LCtx, MDVal);

    Bldr.generateNode(M, Pred, state);
    return;
  }

  // Handle regular struct fields / member variables.
  state = createTemporaryRegionIfNeeded(state, LCtx, BaseExpr);
  SVal baseExprVal = state->getSVal(BaseExpr, LCtx);

  FieldDecl *field = cast<FieldDecl>(Member);
  SVal L = state->getLValue(field, baseExprVal);
  if (M->isGLValue()) {
    if (field->getType()->isReferenceType()) {
      if (const MemRegion *R = L.getAsRegion())
        L = state->getSVal(R);
      else
        L = UnknownVal();
    }

    Bldr.generateNode(M, Pred, state->BindExpr(M, LCtx, L), 0,
                      ProgramPoint::PostLValueKind);
  } else {
    Bldr.takeNodes(Pred);
    evalLoad(Dst, M, M, Pred, state, L);
    Bldr.addNodes(Dst);
  }
}

/// evalBind - Handle the semantics of binding a value to a specific location.
///  This method is used by evalStore and (soon) VisitDeclStmt, and others.
void ExprEngine::evalBind(ExplodedNodeSet &Dst, const Stmt *StoreE,
                          ExplodedNode *Pred,
                          SVal location, SVal Val,
                          bool atDeclInit, const ProgramPoint *PP) {

  const LocationContext *LC = Pred->getLocationContext();
  PostStmt PS(StoreE, LC);
  if (!PP)
    PP = &PS;

  // Do a previsit of the bind.
  ExplodedNodeSet CheckedSet;
  getCheckerManager().runCheckersForBind(CheckedSet, Pred, location, Val,
                                         StoreE, *this, *PP);

  // If the location is not a 'Loc', it will already be handled by
  // the checkers.  There is nothing left to do.
  if (!isa<Loc>(location)) {
    Dst = CheckedSet;
    return;
  }
  
  ExplodedNodeSet TmpDst;
  StmtNodeBuilder Bldr(CheckedSet, TmpDst, *currBldrCtx);

  for (ExplodedNodeSet::iterator I = CheckedSet.begin(), E = CheckedSet.end();
       I!=E; ++I) {
    ExplodedNode *PredI = *I;
    ProgramStateRef state = PredI->getState();
    
    // When binding the value, pass on the hint that this is a initialization.
    // For initializations, we do not need to inform clients of region
    // changes.
    state = state->bindLoc(cast<Loc>(location),
                           Val, /* notifyChanges = */ !atDeclInit);
    
    const MemRegion *LocReg = 0;
    if (loc::MemRegionVal *LocRegVal = dyn_cast<loc::MemRegionVal>(&location)) {
      LocReg = LocRegVal->getRegion();
    }
    
    const ProgramPoint L = PostStore(StoreE, LC, LocReg, 0);
    Bldr.generateNode(L, state, PredI);
  }
  Dst.insert(TmpDst);
}

/// evalStore - Handle the semantics of a store via an assignment.
///  @param Dst The node set to store generated state nodes
///  @param AssignE The assignment expression if the store happens in an
///         assignment.
///  @param LocationE The location expression that is stored to.
///  @param state The current simulation state
///  @param location The location to store the value
///  @param Val The value to be stored
void ExprEngine::evalStore(ExplodedNodeSet &Dst, const Expr *AssignE,
                             const Expr *LocationE,
                             ExplodedNode *Pred,
                             ProgramStateRef state, SVal location, SVal Val,
                             const ProgramPointTag *tag) {
  // Proceed with the store.  We use AssignE as the anchor for the PostStore
  // ProgramPoint if it is non-NULL, and LocationE otherwise.
  const Expr *StoreE = AssignE ? AssignE : LocationE;

  // Evaluate the location (checks for bad dereferences).
  ExplodedNodeSet Tmp;
  evalLocation(Tmp, AssignE, LocationE, Pred, state, location, tag, false);

  if (Tmp.empty())
    return;

  if (location.isUndef())
    return;

  for (ExplodedNodeSet::iterator NI=Tmp.begin(), NE=Tmp.end(); NI!=NE; ++NI)
    evalBind(Dst, StoreE, *NI, location, Val, false);
}

void ExprEngine::evalLoad(ExplodedNodeSet &Dst,
                          const Expr *NodeEx,
                          const Expr *BoundEx,
                          ExplodedNode *Pred,
                          ProgramStateRef state,
                          SVal location,
                          const ProgramPointTag *tag,
                          QualType LoadTy)
{
  assert(!isa<NonLoc>(location) && "location cannot be a NonLoc.");

  // Are we loading from a region?  This actually results in two loads; one
  // to fetch the address of the referenced value and one to fetch the
  // referenced value.
  if (const TypedValueRegion *TR =
        dyn_cast_or_null<TypedValueRegion>(location.getAsRegion())) {

    QualType ValTy = TR->getValueType();
    if (const ReferenceType *RT = ValTy->getAs<ReferenceType>()) {
      static SimpleProgramPointTag
             loadReferenceTag("ExprEngine : Load Reference");
      ExplodedNodeSet Tmp;
      evalLoadCommon(Tmp, NodeEx, BoundEx, Pred, state,
                     location, &loadReferenceTag,
                     getContext().getPointerType(RT->getPointeeType()));

      // Perform the load from the referenced value.
      for (ExplodedNodeSet::iterator I=Tmp.begin(), E=Tmp.end() ; I!=E; ++I) {
        state = (*I)->getState();
        location = state->getSVal(BoundEx, (*I)->getLocationContext());
        evalLoadCommon(Dst, NodeEx, BoundEx, *I, state, location, tag, LoadTy);
      }
      return;
    }
  }

  evalLoadCommon(Dst, NodeEx, BoundEx, Pred, state, location, tag, LoadTy);
}

void ExprEngine::evalLoadCommon(ExplodedNodeSet &Dst,
                                const Expr *NodeEx,
                                const Expr *BoundEx,
                                ExplodedNode *Pred,
                                ProgramStateRef state,
                                SVal location,
                                const ProgramPointTag *tag,
                                QualType LoadTy) {
  assert(NodeEx);
  assert(BoundEx);
  // Evaluate the location (checks for bad dereferences).
  ExplodedNodeSet Tmp;
  evalLocation(Tmp, NodeEx, BoundEx, Pred, state, location, tag, true);
  if (Tmp.empty())
    return;

  StmtNodeBuilder Bldr(Tmp, Dst, *currBldrCtx);
  if (location.isUndef())
    return;

  // Proceed with the load.
  for (ExplodedNodeSet::iterator NI=Tmp.begin(), NE=Tmp.end(); NI!=NE; ++NI) {
    state = (*NI)->getState();
    const LocationContext *LCtx = (*NI)->getLocationContext();

    if (location.isUnknown()) {
      // This is important.  We must nuke the old binding.
      Bldr.generateNode(NodeEx, *NI,
                        state->BindExpr(BoundEx, LCtx, UnknownVal()),
                        tag, ProgramPoint::PostLoadKind);
    }
    else {
      if (LoadTy.isNull())
        LoadTy = BoundEx->getType();
      SVal V = state->getSVal(cast<Loc>(location), LoadTy);
      Bldr.generateNode(NodeEx, *NI,
                        state->bindExprAndLocation(BoundEx, LCtx, location, V),
                        tag, ProgramPoint::PostLoadKind);
    }
  }
}

void ExprEngine::evalLocation(ExplodedNodeSet &Dst,
                              const Stmt *NodeEx,
                              const Stmt *BoundEx,
                              ExplodedNode *Pred,
                              ProgramStateRef state,
                              SVal location,
                              const ProgramPointTag *tag,
                              bool isLoad) {
  StmtNodeBuilder BldrTop(Pred, Dst, *currBldrCtx);
  // Early checks for performance reason.
  if (location.isUnknown()) {
    return;
  }

  ExplodedNodeSet Src;
  BldrTop.takeNodes(Pred);
  StmtNodeBuilder Bldr(Pred, Src, *currBldrCtx);
  if (Pred->getState() != state) {
    // Associate this new state with an ExplodedNode.
    // FIXME: If I pass null tag, the graph is incorrect, e.g for
    //   int *p;
    //   p = 0;
    //   *p = 0xDEADBEEF;
    // "p = 0" is not noted as "Null pointer value stored to 'p'" but
    // instead "int *p" is noted as
    // "Variable 'p' initialized to a null pointer value"
    
    static SimpleProgramPointTag tag("ExprEngine: Location");
    Bldr.generateNode(NodeEx, Pred, state, &tag);
  }
  ExplodedNodeSet Tmp;
  getCheckerManager().runCheckersForLocation(Tmp, Src, location, isLoad,
                                             NodeEx, BoundEx, *this);
  BldrTop.addNodes(Tmp);
}

std::pair<const ProgramPointTag *, const ProgramPointTag*>
ExprEngine::geteagerlyAssumeBinOpBifurcationTags() {
  static SimpleProgramPointTag
         eagerlyAssumeBinOpBifurcationTrue("ExprEngine : Eagerly Assume True"),
         eagerlyAssumeBinOpBifurcationFalse("ExprEngine : Eagerly Assume False");
  return std::make_pair(&eagerlyAssumeBinOpBifurcationTrue,
                        &eagerlyAssumeBinOpBifurcationFalse);
}

void ExprEngine::evalEagerlyAssumeBinOpBifurcation(ExplodedNodeSet &Dst,
                                                   ExplodedNodeSet &Src,
                                                   const Expr *Ex) {
  StmtNodeBuilder Bldr(Src, Dst, *currBldrCtx);
  
  for (ExplodedNodeSet::iterator I=Src.begin(), E=Src.end(); I!=E; ++I) {
    ExplodedNode *Pred = *I;
    // Test if the previous node was as the same expression.  This can happen
    // when the expression fails to evaluate to anything meaningful and
    // (as an optimization) we don't generate a node.
    ProgramPoint P = Pred->getLocation();
    if (!isa<PostStmt>(P) || cast<PostStmt>(P).getStmt() != Ex) {
      continue;
    }

    ProgramStateRef state = Pred->getState();
    SVal V = state->getSVal(Ex, Pred->getLocationContext());
    nonloc::SymbolVal *SEV = dyn_cast<nonloc::SymbolVal>(&V);
    if (SEV && SEV->isExpression()) {
      const std::pair<const ProgramPointTag *, const ProgramPointTag*> &tags =
        geteagerlyAssumeBinOpBifurcationTags();

      // First assume that the condition is true.
      if (ProgramStateRef StateTrue = state->assume(*SEV, true)) {
        SVal Val = svalBuilder.makeIntVal(1U, Ex->getType());        
        StateTrue = StateTrue->BindExpr(Ex, Pred->getLocationContext(), Val);
        Bldr.generateNode(Ex, Pred, StateTrue, tags.first);
      }

      // Next, assume that the condition is false.
      if (ProgramStateRef StateFalse = state->assume(*SEV, false)) {
        SVal Val = svalBuilder.makeIntVal(0U, Ex->getType());
        StateFalse = StateFalse->BindExpr(Ex, Pred->getLocationContext(), Val);
        Bldr.generateNode(Ex, Pred, StateFalse, tags.second);
      }
    }
  }
}

void ExprEngine::VisitGCCAsmStmt(const GCCAsmStmt *A, ExplodedNode *Pred,
                                 ExplodedNodeSet &Dst) {
  StmtNodeBuilder Bldr(Pred, Dst, *currBldrCtx);
  // We have processed both the inputs and the outputs.  All of the outputs
  // should evaluate to Locs.  Nuke all of their values.

  // FIXME: Some day in the future it would be nice to allow a "plug-in"
  // which interprets the inline asm and stores proper results in the
  // outputs.

  ProgramStateRef state = Pred->getState();

  for (GCCAsmStmt::const_outputs_iterator OI = A->begin_outputs(),
       OE = A->end_outputs(); OI != OE; ++OI) {
    SVal X = state->getSVal(*OI, Pred->getLocationContext());
    assert (!isa<NonLoc>(X));  // Should be an Lval, or unknown, undef.

    if (isa<Loc>(X))
      state = state->bindLoc(cast<Loc>(X), UnknownVal());
  }

  Bldr.generateNode(A, Pred, state);
}

void ExprEngine::VisitMSAsmStmt(const MSAsmStmt *A, ExplodedNode *Pred,
                                ExplodedNodeSet &Dst) {
  StmtNodeBuilder Bldr(Pred, Dst, *currBldrCtx);
  Bldr.generateNode(A, Pred, Pred->getState());
}

//===----------------------------------------------------------------------===//
// Visualization.
//===----------------------------------------------------------------------===//

#ifndef NDEBUG
static ExprEngine* GraphPrintCheckerState;
static SourceManager* GraphPrintSourceManager;

namespace llvm {
template<>
struct DOTGraphTraits<ExplodedNode*> :
  public DefaultDOTGraphTraits {

  DOTGraphTraits (bool isSimple=false) : DefaultDOTGraphTraits(isSimple) {}

  // FIXME: Since we do not cache error nodes in ExprEngine now, this does not
  // work.
  static std::string getNodeAttributes(const ExplodedNode *N, void*) {

#if 0
      // FIXME: Replace with a general scheme to tell if the node is
      // an error node.
    if (GraphPrintCheckerState->isImplicitNullDeref(N) ||
        GraphPrintCheckerState->isExplicitNullDeref(N) ||
        GraphPrintCheckerState->isUndefDeref(N) ||
        GraphPrintCheckerState->isUndefStore(N) ||
        GraphPrintCheckerState->isUndefControlFlow(N) ||
        GraphPrintCheckerState->isUndefResult(N) ||
        GraphPrintCheckerState->isBadCall(N) ||
        GraphPrintCheckerState->isUndefArg(N))
      return "color=\"red\",style=\"filled\"";

    if (GraphPrintCheckerState->isNoReturnCall(N))
      return "color=\"blue\",style=\"filled\"";
#endif
    return "";
  }

  static void printLocation(llvm::raw_ostream &Out, SourceLocation SLoc) {
    if (SLoc.isFileID()) {
      Out << "\\lline="
        << GraphPrintSourceManager->getExpansionLineNumber(SLoc)
        << " col="
        << GraphPrintSourceManager->getExpansionColumnNumber(SLoc)
        << "\\l";
    }
  }

  static std::string getNodeLabel(const ExplodedNode *N, void*){

    std::string sbuf;
    llvm::raw_string_ostream Out(sbuf);

    // Program Location.
    ProgramPoint Loc = N->getLocation();

    switch (Loc.getKind()) {
      case ProgramPoint::BlockEntranceKind: {
        Out << "Block Entrance: B"
            << cast<BlockEntrance>(Loc).getBlock()->getBlockID();
        if (const NamedDecl *ND =
                    dyn_cast<NamedDecl>(Loc.getLocationContext()->getDecl())) {
          Out << " (";
          ND->printName(Out);
          Out << ")";
        }
        break;
      }

      case ProgramPoint::BlockExitKind:
        assert (false);
        break;

      case ProgramPoint::CallEnterKind:
        Out << "CallEnter";
        break;

      case ProgramPoint::CallExitBeginKind:
        Out << "CallExitBegin";
        break;

      case ProgramPoint::CallExitEndKind:
        Out << "CallExitEnd";
        break;

      case ProgramPoint::PostStmtPurgeDeadSymbolsKind:
        Out << "PostStmtPurgeDeadSymbols";
        break;

      case ProgramPoint::PreStmtPurgeDeadSymbolsKind:
        Out << "PreStmtPurgeDeadSymbols";
        break;

      case ProgramPoint::EpsilonKind:
        Out << "Epsilon Point";
        break;

      case ProgramPoint::PreImplicitCallKind: {
        ImplicitCallPoint *PC = cast<ImplicitCallPoint>(&Loc);
        Out << "PreCall: ";

        // FIXME: Get proper printing options.
        PC->getDecl()->print(Out, LangOptions());
        printLocation(Out, PC->getLocation());
        break;
      }

      case ProgramPoint::PostImplicitCallKind: {
        ImplicitCallPoint *PC = cast<ImplicitCallPoint>(&Loc);
        Out << "PostCall: ";

        // FIXME: Get proper printing options.
        PC->getDecl()->print(Out, LangOptions());
        printLocation(Out, PC->getLocation());
        break;
      }

      default: {
        if (StmtPoint *L = dyn_cast<StmtPoint>(&Loc)) {
          const Stmt *S = L->getStmt();

          Out << S->getStmtClassName() << ' ' << (const void*) S << ' ';
          LangOptions LO; // FIXME.
          S->printPretty(Out, 0, PrintingPolicy(LO));
          printLocation(Out, S->getLocStart());

          if (isa<PreStmt>(Loc))
            Out << "\\lPreStmt\\l;";
          else if (isa<PostLoad>(Loc))
            Out << "\\lPostLoad\\l;";
          else if (isa<PostStore>(Loc))
            Out << "\\lPostStore\\l";
          else if (isa<PostLValue>(Loc))
            Out << "\\lPostLValue\\l";

#if 0
            // FIXME: Replace with a general scheme to determine
            // the name of the check.
          if (GraphPrintCheckerState->isImplicitNullDeref(N))
            Out << "\\|Implicit-Null Dereference.\\l";
          else if (GraphPrintCheckerState->isExplicitNullDeref(N))
            Out << "\\|Explicit-Null Dereference.\\l";
          else if (GraphPrintCheckerState->isUndefDeref(N))
            Out << "\\|Dereference of undefialied value.\\l";
          else if (GraphPrintCheckerState->isUndefStore(N))
            Out << "\\|Store to Undefined Loc.";
          else if (GraphPrintCheckerState->isUndefResult(N))
            Out << "\\|Result of operation is undefined.";
          else if (GraphPrintCheckerState->isNoReturnCall(N))
            Out << "\\|Call to function marked \"noreturn\".";
          else if (GraphPrintCheckerState->isBadCall(N))
            Out << "\\|Call to NULL/Undefined.";
          else if (GraphPrintCheckerState->isUndefArg(N))
            Out << "\\|Argument in call is undefined";
#endif

          break;
        }

        const BlockEdge &E = cast<BlockEdge>(Loc);
        Out << "Edge: (B" << E.getSrc()->getBlockID() << ", B"
            << E.getDst()->getBlockID()  << ')';

        if (const Stmt *T = E.getSrc()->getTerminator()) {

          SourceLocation SLoc = T->getLocStart();

          Out << "\\|Terminator: ";
          LangOptions LO; // FIXME.
          E.getSrc()->printTerminator(Out, LO);

          if (SLoc.isFileID()) {
            Out << "\\lline="
              << GraphPrintSourceManager->getExpansionLineNumber(SLoc)
              << " col="
              << GraphPrintSourceManager->getExpansionColumnNumber(SLoc);
          }

          if (isa<SwitchStmt>(T)) {
            const Stmt *Label = E.getDst()->getLabel();

            if (Label) {
              if (const CaseStmt *C = dyn_cast<CaseStmt>(Label)) {
                Out << "\\lcase ";
                LangOptions LO; // FIXME.
                C->getLHS()->printPretty(Out, 0, PrintingPolicy(LO));

                if (const Stmt *RHS = C->getRHS()) {
                  Out << " .. ";
                  RHS->printPretty(Out, 0, PrintingPolicy(LO));
                }

                Out << ":";
              }
              else {
                assert (isa<DefaultStmt>(Label));
                Out << "\\ldefault:";
              }
            }
            else
              Out << "\\l(implicit) default:";
          }
          else if (isa<IndirectGotoStmt>(T)) {
            // FIXME
          }
          else {
            Out << "\\lCondition: ";
            if (*E.getSrc()->succ_begin() == E.getDst())
              Out << "true";
            else
              Out << "false";
          }

          Out << "\\l";
        }

#if 0
          // FIXME: Replace with a general scheme to determine
          // the name of the check.
        if (GraphPrintCheckerState->isUndefControlFlow(N)) {
          Out << "\\|Control-flow based on\\lUndefined value.\\l";
        }
#endif
      }
    }

    ProgramStateRef state = N->getState();
    Out << "\\|StateID: " << (const void*) state.getPtr()
        << " NodeID: " << (const void*) N << "\\|";
    state->printDOT(Out);

    Out << "\\l";    

    if (const ProgramPointTag *tag = Loc.getTag()) {
      Out << "\\|Tag: " << tag->getTagDescription(); 
      Out << "\\l";
    }
    return Out.str();
  }
};
} // end llvm namespace
#endif

#ifndef NDEBUG
template <typename ITERATOR>
ExplodedNode *GetGraphNode(ITERATOR I) { return *I; }

template <> ExplodedNode*
GetGraphNode<llvm::DenseMap<ExplodedNode*, Expr*>::iterator>
  (llvm::DenseMap<ExplodedNode*, Expr*>::iterator I) {
  return I->first;
}
#endif

void ExprEngine::ViewGraph(bool trim) {
#ifndef NDEBUG
  if (trim) {
    std::vector<ExplodedNode*> Src;

    // Flush any outstanding reports to make sure we cover all the nodes.
    // This does not cause them to get displayed.
    for (BugReporter::iterator I=BR.begin(), E=BR.end(); I!=E; ++I)
      const_cast<BugType*>(*I)->FlushReports(BR);

    // Iterate through the reports and get their nodes.
    for (BugReporter::EQClasses_iterator
           EI = BR.EQClasses_begin(), EE = BR.EQClasses_end(); EI != EE; ++EI) {
      ExplodedNode *N = const_cast<ExplodedNode*>(EI->begin()->getErrorNode());
      if (N) Src.push_back(N);
    }

    ViewGraph(&Src[0], &Src[0]+Src.size());
  }
  else {
    GraphPrintCheckerState = this;
    GraphPrintSourceManager = &getContext().getSourceManager();

    llvm::ViewGraph(*G.roots_begin(), "ExprEngine");

    GraphPrintCheckerState = NULL;
    GraphPrintSourceManager = NULL;
  }
#endif
}

void ExprEngine::ViewGraph(ExplodedNode** Beg, ExplodedNode** End) {
#ifndef NDEBUG
  GraphPrintCheckerState = this;
  GraphPrintSourceManager = &getContext().getSourceManager();

  std::auto_ptr<ExplodedGraph> TrimmedG(G.Trim(Beg, End).first);

  if (!TrimmedG.get())
    llvm::errs() << "warning: Trimmed ExplodedGraph is empty.\n";
  else
    llvm::ViewGraph(*TrimmedG->roots_begin(), "TrimmedExprEngine");

  GraphPrintCheckerState = NULL;
  GraphPrintSourceManager = NULL;
#endif
}
