//===-- llvm/Analysis/Passes.h - Constructors for analyses ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header file defines prototypes for accessor functions that expose passes
// in the analysis libraries.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_PASSES_H
#define LLVM_ANALYSIS_PASSES_H

namespace llvm {
  class FunctionPass;
  class ImmutablePass;
  class LoopPass;
  class ModulePass;
  class Pass;
  class PassInfo;
  class LibCallInfo;

  //===--------------------------------------------------------------------===//
  //
  // createGlobalsModRefPass - This pass provides alias and mod/ref info for
  // global values that do not have their addresses taken.
  //
  Pass *createGlobalsModRefPass();

  //===--------------------------------------------------------------------===//
  //
  // createAliasDebugger - This pass helps debug clients of AA
  //
  Pass *createAliasDebugger();

  //===--------------------------------------------------------------------===//
  //
  // createAliasAnalysisCounterPass - This pass counts alias queries and how the
  // alias analysis implementation responds.
  //
  ModulePass *createAliasAnalysisCounterPass();

  //===--------------------------------------------------------------------===//
  //
  // createAAEvalPass - This pass implements a simple N^2 alias analysis
  // accuracy evaluator.
  //
  FunctionPass *createAAEvalPass();

  //===--------------------------------------------------------------------===//
  //
  // createNoAAPass - This pass implements a "I don't know" alias analysis.
  //
  ImmutablePass *createNoAAPass();

  //===--------------------------------------------------------------------===//
  //
  // createBasicAliasAnalysisPass - This pass implements the default alias
  // analysis.
  //
  ImmutablePass *createBasicAliasAnalysisPass();

  //===--------------------------------------------------------------------===//
  //
  /// createLibCallAliasAnalysisPass - Create an alias analysis pass that knows
  /// about the semantics of a set of libcalls specified by LCI.  The newly
  /// constructed pass takes ownership of the pointer that is provided.
  ///
  FunctionPass *createLibCallAliasAnalysisPass(LibCallInfo *LCI);

  //===--------------------------------------------------------------------===//
  //
  // createScalarEvolutionAliasAnalysisPass - This pass implements a simple
  // alias analysis using ScalarEvolution queries.
  //
  FunctionPass *createScalarEvolutionAliasAnalysisPass();

  //===--------------------------------------------------------------------===//
  //
  // createProfileLoaderPass - This pass loads information from a profile dump
  // file.
  //
  ModulePass *createProfileLoaderPass();
  extern const PassInfo *ProfileLoaderPassID;

  //===--------------------------------------------------------------------===//
  //
  // createNoProfileInfoPass - This pass implements the default "no profile".
  //
  ImmutablePass *createNoProfileInfoPass();

  //===--------------------------------------------------------------------===//
  //
  // createProfileEstimatorPass - This pass estimates profiling information
  // instead of loading it from a previous run.
  //
  FunctionPass *createProfileEstimatorPass();
  extern const PassInfo *ProfileEstimatorPassID;

  //===--------------------------------------------------------------------===//
  //
  // createProfileVerifierPass - This pass verifies profiling information.
  //
  FunctionPass *createProfileVerifierPass();

  //===--------------------------------------------------------------------===//
  //
  // createDSAAPass - This pass implements simple context sensitive alias
  // analysis.
  //
  ModulePass *createDSAAPass();

  //===--------------------------------------------------------------------===//
  //
  // createDSOptPass - This pass uses DSA to do a series of simple
  // optimizations.
  //
  ModulePass *createDSOptPass();

  //===--------------------------------------------------------------------===//
  //
  // createSteensgaardPass - This pass uses the data structure graphs to do a
  // simple context insensitive alias analysis.
  //
  ModulePass *createSteensgaardPass();

  //===--------------------------------------------------------------------===//
  //
  // createLiveValuesPass - This creates an instance of the LiveValues pass.
  //
  FunctionPass *createLiveValuesPass();
  
  //===--------------------------------------------------------------------===//
  //
  /// createLazyValueInfoPass - This creates an instance of the LazyValueInfo
  /// pass.
  FunctionPass *createLazyValueInfoPass();

  //===--------------------------------------------------------------------===//
  //
  // createLoopDependenceAnalysisPass - This creates an instance of the
  // LoopDependenceAnalysis pass.
  //
  LoopPass *createLoopDependenceAnalysisPass();
  
  // Minor pass prototypes, allowing us to expose them through bugpoint and
  // analyze.
  FunctionPass *createInstCountPass();

  // print debug info intrinsics in human readable form
  FunctionPass *createDbgInfoPrinterPass();

  // Print module-level debug info metadata in human-readable form.
  ModulePass *createModuleDebugInfoPrinterPass();
}

#endif
