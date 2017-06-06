//===-- CPI.cpp - CPI and CPS Instrumentation Pass ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass inserts CPI or CPS instumentation
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "cpi"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Pass.h"
#include "llvm/DebugInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/TargetFolder.h"
#include "llvm/Support/Debug.h"

#include <vector>

// Uncomment the following to enable the runtime stats collection
// instrumentation. Remember to enable in cpi.cc in compiler-rt as well
// Both switches must be active or not at the same time!
//#define CPI_PROFILE_STATS

using namespace llvm;

// Validate the result of Module::getOrInsertFunction called for an interface
// function of CPI. If the instrumented module defines a function
// with the same name, their prototypes must match, otherwise
// getOrInsertFunction returns a bitcast.
static Function *CheckInterfaceFunction(Constant *FuncOrBitcast) {
  if (isa<Function>(FuncOrBitcast)) return cast<Function>(FuncOrBitcast);
  FuncOrBitcast->dump();
  report_fatal_error("trying to redefine an CPI "
                     "interface function");
}

namespace {

// XXX: increase this!
#define CPI_LOOP_UNROLL_TRESHOLD 2

  cl::opt<bool> SoftwareMode("cpi-software",
        cl::desc("Apply software-based CPI or CPS"),
        cl::init(false));

  cl::opt<bool> ShowStats("cpi-stats",
        cl::desc("Show CPI compile-time statistics"),
        cl::init(false));

  cl::opt<bool> CPIDebugMode("cpi-debug",
        cl::desc("Enable CPI debug mode"),
        cl::init(
#ifdef __FreeBSD__
                 true
#else
                 false
#endif
                 ));

  STATISTIC(NumStores, "Total number of memory stores");
  STATISTIC(NumProtectedStores, "Number of protected memory stores");

  STATISTIC(NumLoads, "Total number of memory loads");
  STATISTIC(NumProtectedLoads, "Number of protected memory loads");

  STATISTIC(NumBoundsChecks, "Number of bounds checks");

  STATISTIC(NumMemcpyLikeOps, "Total number of memcpy-like operations");
  STATISTIC(NumProtectedMemcpyLikeOps,
            "Number of protected memcpy-like operations");

  STATISTIC(NumAllocFreeOps, "Total number of alloc- and free-like operations");
  STATISTIC(NumProtectedAllocFreeOps,
            "Number of protected alloc- and free-like operations");

  STATISTIC(NumInitStores,
            "Total number of all initialization stores");
  STATISTIC(NumProtectedInitStores,
            "Number of all protected initialization stores");

  STATISTIC(NumCalls, "Total number of function calls");
  STATISTIC(NumIndirectCalls, "Total number of indirect function calls");

  static void PrintStat(raw_ostream &OS, Statistic &S) {
    OS << format("%8u %s - %s\n", S.getValue(), S.getName(), S.getDesc());
  }

  struct CPIInterfaceFunctions {
    Function *CPIInitFn;

    Function *CPISetFn;
    Function *CPIAssertFn;

    Function *CPISetBoundsFn;
    Function *CPIAssertBoundsFn;

    Function *CPIGetMetadataFn;
    Function *CPIGetMetadataNocheckFn;
    Function *CPIGetValFn;
    Function *CPIGetBoundsFn;

    Function *CPISetArgBoundsFn;
    Function *CPIGetArgBoundsFn;

    Function *CPIDeleteRangeFn;
    Function *CPICopyRangeFn;
    Function *CPIMoveRangeFn;

    Function *CPIMallocSizeFn;
    Function *CPIAllocFn;
    Function *CPIReallocFn;
    Function *CPIFreeFn;

#ifdef CPI_PROFILE_STATS
    Function *CPIRegisterProfileTable;
#endif

    Function *CPIDumpFn;
  };

  class CPIPrepare : public ModulePass {
  public:
    static char ID;

    CPIPrepare() : ModulePass(ID) {
      initializeCPIPreparePass(*PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DataLayout>();
    }

    bool runOnModule(Module &M);
  };

  /** A pass that instruments every load/store that can modify pointer in a
      transitive closure of function pointers under the points-to
      relationship. */
  class CPI : public ModulePass {
    DataLayout *DL;
    TargetLibraryInfo *TLI;
    AliasAnalysis *AA;

    CPIInterfaceFunctions IF;

    bool HasCPIFullFunctions;

    DenseMap<StructType*, MDNode*> StructsTBAA;
    DenseMap<StructType*, MDNode*> UnionsTBAA;

    IntegerType *IntPtrTy;
    VectorType *BoundsTy;
    StructType *PtrValBoundsTy;
    Constant *InftyBounds;
    Constant *EmptyBounds;

    DenseMap<Function*, bool> MayBeCalledExternally;

    typedef DenseMap<PointerIntPair<Type*, 1>, bool> TypesProtectInfoTy;
    TypesProtectInfoTy StructTypesProtectInfo;
    bool shouldProtectType(Type *Ty, bool IsStore, bool CPSOnly,
                           MDNode *TBAATag = NULL);

    // Check whether a pointer Ptr needs protection
    bool shouldProtectValue(Value *Val, bool IsStore, bool CPSOnly,
                            MDNode *TBAATag = NULL, Type *RealType = NULL);

    // Check whether storage location pointed to by Ptr needs protection
    bool shouldProtectLoc(Value *Ptr, bool IsStore);

    bool pointsToVTable(Value *Ptr);
    bool isUsedInProtectContext(Value *Ptr, bool CPSOnly);

    void buildMetadataReload(IRBuilder<true, TargetFolder> &IRB, Value *VPtr,
                             Value *EndPtr, BasicBlock *ExitBB, Value *PPt);

    void buildMetadataReloadLoop(IRBuilder<true, TargetFolder> &IRB,
                                 Value *VPtr, Value *Size, Value *PPt);

    /// Create bounds with a given base and size
    Value *createBounds(IRBuilder<> &IRB, Value *Base, Value *Size);

    /// Insert a bounds load and a check for value V. It V is a load and is not
    /// in NeedBounds yet, add it to NeedBounds ad BoundsChecks arrays.
    Value *insertBoundsAndChecks(Value *V, DenseMap<Value*, Value*> &BM,
                                 SetVector<Value*> &NeedBounds,
                                 std::vector<std::pair<Instruction*,
                                  std::pair<Value*,uint64_t> > > &BoundsChecks,
                                 SmallPtrSet<Value*, 64> &IsDereferenced,
                                 SetVector<std::pair<Instruction*,
                                  Instruction*> > &ReplMap);

    void insertChecks(DenseMap<Value*, Value*> &BM,
                      Value *V, bool IsDereferenced,
                      SetVector<std::pair<Instruction*,
                                          Instruction*> > &ReplMap);

    void decorateInstruction(Instruction *I, LLVMContext &C) {
      MDNode *TBAATag = I->getMetadata(LLVMContext::MD_tbaa);
      if (TBAATag) {
        StringRef T = cast<MDString>(TBAATag->getOperand(0))->getString();
        // vtable pointer needs no more decoration
        if (T == "vtable pointer")
          return;
      }
      // XXX is ok to overwrite?
      MDNode *MD = MDNode::get(C, MDString::get(C, "vregion1"));
      I->setMetadata(LLVMContext::MD_tbaa, MD);
    }

  public:
    static char ID;             // Pass identification, replacement for typeid.
    CPI() : ModulePass(ID), HasCPIFullFunctions(false) {
      initializeCPIPass(*PassRegistry::getPassRegistry());
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DataLayout>();
      AU.addRequired<TargetLibraryInfo>();
      AU.addRequired<AliasAnalysis>();
    }

    bool runOnFunction(Function &F);
    bool runOnFunctionBounds(Function &F);
    bool doCPIInitialization(Module &M);
    bool doCPIFinalization(Module &M);

    Function *createGlobalsReload(Module &M, StringRef N,
                                  bool OnlyDeclarations);

    bool mayBeCalledExternally(Function *F);

    virtual bool runOnModule(Module &M) {
      DL = &getAnalysis<DataLayout>();
      TLI = &getAnalysis<TargetLibraryInfo>();
      AA = &getAnalysis<AliasAnalysis>();

      NamedMDNode *STBAA = M.getNamedMetadata("clang.tbaa.structs");
      for (size_t i = 0, e = STBAA->getNumOperands(); i != e; ++i) {
        MDNode *MD = STBAA->getOperand(i);
        MDNode *TBAATag = dyn_cast_or_null<MDNode>(MD->getOperand(1));
        if (TBAATag)
          StructsTBAA[cast<StructType>(MD->getOperand(0)->getType())] = TBAATag;
      }

      NamedMDNode *UTBAA = M.getNamedMetadata("clang.tbaa.unions");
      for (size_t i = 0, e = UTBAA->getNumOperands(); i != e; ++i) {
        MDNode *MD = UTBAA->getOperand(i);
        MDNode *TBAATag = dyn_cast_or_null<MDNode>(MD->getOperand(1));
        if (TBAATag)
          UnionsTBAA[cast<StructType>(MD->getOperand(0)->getType())] = TBAATag;
      }

      HasCPIFullFunctions = false;

      IntPtrTy = DL->getIntPtrType(M.getContext());
      BoundsTy = VectorType::get(IntPtrTy, 2);
      PtrValBoundsTy = StructType::get(IntPtrTy, IntPtrTy, BoundsTy, NULL);

      uint64_t InftyBoundsArr[2] = { 0ULL, ~0ULL };
      uint64_t EmptyBoundsArr[2] = { ~0ULL, 0ULL };
      InftyBounds = ConstantDataVector::get(M.getContext(), InftyBoundsArr);
      EmptyBounds = ConstantDataVector::get(M.getContext(), EmptyBoundsArr);

      doCPIInitialization(M);

      // Fill in MayBeCalledExternally map
      for (Module::iterator It = M.begin(), Ie = M.end(); It != Ie; ++It) {
        Function *F = &*It;
        MayBeCalledExternally[F] = mayBeCalledExternally(F);
      }

      for (Module::iterator It = M.begin(), Ie = M.end(); It != Ie; ++It) {
        Function &F = *It;
        if (!F.isDeclaration() && !F.getName().startswith("llvm.") &&
            !F.getName().startswith("__llvm__")) {
          runOnFunction(F);
        }
      }

      doCPIFinalization(M);

      if (ShowStats) {
        outs() << "CPI FPTR Statistics:\n";

        PrintStat(outs(), NumCalls);
        PrintStat(outs(), NumIndirectCalls);

        PrintStat(outs(), NumStores);
        PrintStat(outs(), NumProtectedStores);
        PrintStat(outs(), NumLoads);
        PrintStat(outs(), NumProtectedLoads);
        PrintStat(outs(), NumInitStores);
        PrintStat(outs(), NumProtectedInitStores);

        PrintStat(outs(), NumBoundsChecks);

        PrintStat(outs(), NumMemcpyLikeOps);
        PrintStat(outs(), NumProtectedMemcpyLikeOps);
        PrintStat(outs(), NumAllocFreeOps);
        PrintStat(outs(), NumProtectedAllocFreeOps);
      }

      return true;
    }

#ifdef CPI_PROFILE_STATS
  private:
    GlobalVariable *ProfileTable;
    std::vector<std::string> ProfileNames;

#if 0
    static void PrintDebugLoc(LLVMContext &Ctx, const DebugLoc& DbgLoc,
                              raw_ostream &Outs) {
      if (DbgLoc.isUnknown()) {
        Outs << "<debug info not available>";
        return;
      }

      MDNode *Scope, *InlinedAt;
      DbgLoc.getScopeAndInlinedAt(Scope, InlinedAt, Ctx);

      StringRef Filename = DIScope(Scope).getFilename();
      Filename = sys::path::filename(Filename);

      Outs << Filename << ':' << DbgLoc.getLine();

      if (DbgLoc.getCol() != 0)
        Outs << ':' << DbgLoc.getCol();

      if (InlinedAt) {
        Outs << " @ ";
        PrintDebugLoc(Ctx, DebugLoc::getFromDILocation(InlinedAt), Outs);
      }
    }
#endif

    bool doStatsInitialization(Module &M) {
      // We don't know the size of the array, hence we have to use indirection
      LLVMContext &Ctx = M.getContext();
      Type *STy = StructType::get(Type::getInt64Ty(Ctx),
                                  Type::getInt8PtrTy(Ctx), NULL);
      ProfileTable = new GlobalVariable(M, STy, false,
                                        GlobalValue::InternalLinkage, NULL);
      return true;
    }

    template<typename _IRBuilder>
    Value *insertProfilePoint(_IRBuilder &IRB, Instruction *I,
                              Twine Kind, Value *Num = NULL) {
      size_t n = ProfileNames.size();
      Value *Idx[2] = { IRB.getInt64(n), IRB.getInt32(0) };
      Value *P = IRB.CreateGEP(ProfileTable, Idx);

      Value *Inc = Num ? IRB.CreateZExt(Num, IRB.getInt64Ty())
                       : IRB.getInt64(1);
      IRB.CreateAtomicRMW(AtomicRMWInst::Add, P, Inc, Monotonic);

      std::string _s; raw_string_ostream os(_s);
      os << I->getParent()->getParent()->getName() << "\t";

      DebugLoc DL = I->getDebugLoc();
      if (DL.isUnknown()) os << "?" << n;
      else {
        os << DIScope(DL.getScope(I->getContext())).getFilename()
           << ":" << DL.getLine() << ":" << DL.getCol();
        for (DebugLoc InlinedAtDL = DL;;) {
          InlinedAtDL = DebugLoc::getFromDILocation(
                InlinedAtDL.getInlinedAt(I->getContext()));
          if (InlinedAtDL.isUnknown())
            break;
          os << " @ ";
          os << DIScope(InlinedAtDL.getScope(I->getContext())).getFilename()
             << ":" << InlinedAtDL.getLine() << ":" << InlinedAtDL.getCol();
        }
      }

      os << "\t" << Kind;
      ProfileNames.push_back(os.str());

      return P;
    }

    template<typename _IRBuilder>
    void incrementProfilePoint(_IRBuilder &IRB, Value *P) {
      IRB.CreateAtomicRMW(AtomicRMWInst::Add, P, IRB.getInt64(1), Monotonic);
    }

    bool doStatsFinalization(Module &M) {
      // Create the profile table
      LLVMContext &Ctx = M.getContext();
      Type *Int64Ty = Type::getInt64Ty(Ctx);
      Type *VoidPtrTy = Type::getInt8PtrTy(Ctx);

      StructType *PfItemTy = StructType::get(Int64Ty, VoidPtrTy, NULL);
      ArrayType *PfArrayTy = ArrayType::get(PfItemTy, ProfileNames.size());

      SmallVector<Constant*, 32> PfArrayArgs;

      for (size_t i = 0; i < ProfileNames.size(); ++i) {
        Constant *Str = ConstantDataArray::getString(Ctx, ProfileNames[i]);
        GlobalVariable *GV = new GlobalVariable(M, Str->getType(), true,
                                                GlobalValue::InternalLinkage,
                                                Str);
        GV->setUnnamedAddr(true);

        Constant* A[] = {
          ConstantInt::get(Int64Ty, 0),
          ConstantExpr::getPointerCast(GV, VoidPtrTy)
        };

        PfArrayArgs.push_back(ConstantStruct::get(PfItemTy, A));
      }

      GlobalVariable *PfArray =
          new GlobalVariable(M, PfArrayTy, false,
                             GlobalValue::PrivateLinkage,
                             ConstantArray::get(PfArrayTy, PfArrayArgs),
                             "__llvm__cpi_module_profile_table");

      ProfileTable->replaceAllUsesWith(
          ConstantExpr::getBitCast(PfArray, PfItemTy->getPointerTo()));
      ProfileTable->eraseFromParent();

      // Create ctor function
      Function *F = Function::Create(
          FunctionType::get(Type::getVoidTy(Ctx), false),
          GlobalValue::InternalLinkage, "__llvm__cpi_module_profile_ctor", &M);

      SmallVector<Value*, 2> Args;
      Args.push_back(ConstantExpr::getBitCast(PfArray, VoidPtrTy));
      Args.push_back(ConstantInt::get(Int64Ty, ProfileNames.size()));

      BasicBlock *BB = BasicBlock::Create(Ctx, Twine(), F);
      CallInst::Create(IF.CPIRegisterProfileTable, Args, Twine(), BB);
      ReturnInst::Create(Ctx, BB);

      appendToGlobalCtors(M, F, 9999);

      return true;
    }
#else
    template<class _IRBuilder>
    Value *insertProfilePoint(_IRBuilder&, Instruction*,
                              Twine, Value* V = NULL) { return NULL; }
    template<typename _IRBuilder>
    void incrementProfilePoint(_IRBuilder&, Value*) {}
    bool doStatsInitialization(Module &M) { return false; }
    bool doStatsFinalization(Module &M) { return false; }
#endif
  };
} // end anonymous namespace

char CPIPrepare::ID = 0;
INITIALIZE_PASS(CPIPrepare, "cpi-prepare", "CPI preparation pass", false, false)

Pass *llvm::createCPIPreparePass() {
  return new CPIPrepare();
}

char CPI::ID = 0;
INITIALIZE_PASS(CPI, "cpi", "CPI instrumentation pass", false, false)

Pass *llvm::createCPIPass() {
  return new CPI();
}

static void CreateCPIInterfaceFunctions(DataLayout *DL, Module &M,
                                         CPIInterfaceFunctions &IF) {
  LLVMContext &C = M.getContext();
  Type *VoidTy = Type::getVoidTy(C);
  Type *Int8PtrTy = Type::getInt8PtrTy(C);
  Type *Int8PtrPtrTy = Int8PtrTy->getPointerTo();

  Type *Int32Ty = Type::getInt32Ty(C);
  Type *IntPtrTy = DL->getIntPtrType(C);
  Type *SizeTy = IntPtrTy;

  Type *BoundsTy = VectorType::get(IntPtrTy, 2);
  //Type *PtrValBoundsTy = StructType::get(IntPtrTy, IntPtrTy, BoundsTy, NULL);

  IF.CPIInitFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_init", VoidTy, NULL));

  IF.CPISetFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_set", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));

  IF.CPIAssertFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_assert", BoundsTy, Int8PtrPtrTy,
      Int8PtrTy, Int8PtrTy, NULL));

  IF.CPISetBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_set_bounds", VoidTy, Int8PtrPtrTy, Int8PtrTy,
                                        BoundsTy, NULL));

  IF.CPIAssertBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_assert_bounds", VoidTy, Int8PtrTy, SizeTy,
                                           BoundsTy, Int8PtrTy, NULL));

  IF.CPIGetMetadataFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_get_metadata", Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetMetadataNocheckFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_get_metadata_nocheck", Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetValFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_get_val", Int8PtrTy, Int8PtrTy, NULL));

  IF.CPIGetBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_get_bounds", BoundsTy, Int8PtrTy, NULL));

  IF.CPISetArgBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_set_arg_bounds", VoidTy, Int32Ty, BoundsTy, NULL));

  IF.CPIGetArgBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_get_arg_bounds", BoundsTy, Int32Ty, NULL));

  IF.CPIDeleteRangeFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_delete_range", VoidTy, Int8PtrTy, SizeTy, NULL));

  IF.CPICopyRangeFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_copy_range", VoidTy, Int8PtrTy, Int8PtrTy, SizeTy, NULL));

  IF.CPIMoveRangeFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_move_range", VoidTy, Int8PtrTy, Int8PtrTy, SizeTy, NULL));

  IF.CPIMallocSizeFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_malloc_size", SizeTy, Int8PtrTy, NULL));

  IF.CPIAllocFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_alloc", VoidTy, Int8PtrTy, NULL));

  IF.CPIReallocFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_realloc", VoidTy, Int8PtrTy, SizeTy,
                                     Int8PtrTy, SizeTy, NULL));

  IF.CPIFreeFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_free", VoidTy, Int8PtrTy, NULL));

#ifdef CPI_PROFILE_STATS
  IF.CPIRegisterProfileTable = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_register_profile_table", VoidTy, Int8PtrTy, SizeTy, NULL));
#endif

  IF.CPIDumpFn = CheckInterfaceFunction(M.getOrInsertFunction(
      "__llvm__cpi_dump", VoidTy, Int8PtrPtrTy, NULL));
}

bool CPIPrepare::runOnModule(Module &M) {
  const unsigned NumCPIGVs = sizeof(CPIInterfaceFunctions)/sizeof(Function*);
  union {
    CPIInterfaceFunctions IF;
    GlobalValue *GV[NumCPIGVs];
  };

  CreateCPIInterfaceFunctions(&getAnalysis<DataLayout>(), M, IF);

  Type *Int8PtrTy = Type::getInt8PtrTy(M.getContext());
  for (unsigned i = 0; i < NumCPIGVs; ++i) {
    if (GV[i]) appendToGlobalArray(M, "llvm.compiler.used",
                        ConstantExpr::getBitCast(GV[i], Int8PtrTy));
  }

  M.getGlobalVariable("llvm.compiler.used")->setSection("llvm.metadata");

  return true;
}

static MDNode *getNextElTBAATag(size_t &STBAAIndex, Type *ElTy,
                                const StructLayout *SL, unsigned idx,
                                MDNode *STBAATag) {
  if (ElTy->isSingleValueType() && STBAATag) {
    size_t Off = SL->getElementOffset(idx);
    size_t STBAASize = STBAATag->getNumOperands();

    // skip over embedded structs (if any)
    while (STBAAIndex+2 < STBAASize &&
           cast<ConstantInt>(STBAATag->getOperand(STBAAIndex))
              ->getValue().ult(Off)) STBAAIndex += 3;

    if (STBAAIndex+2 < STBAASize &&
        cast<ConstantInt>(STBAATag->getOperand(STBAAIndex))
          ->equalsInt(Off)) {
      // The struct type might be union, in which case we'll have >1 tags
      // for the same offset.
      if (STBAAIndex+3+2 < STBAASize &&
          cast<ConstantInt>(STBAATag->getOperand(STBAAIndex+3))
            ->equalsInt(Off)) {
        // FIXME: support unions
      } else {
        //FIXME: the following assertion seems to not hold for bitfields
        //assert(cast<ConstantInt>(STBAATag->getOperand(STBAAIndex+1))
        //       ->equalsInt(DL->getTypeAllocSize(ElTy)));
        return cast<MDNode>(STBAATag->getOperand(STBAAIndex+2));
      }
    }
  }

  return NULL;
}

bool CPI::shouldProtectType(Type *Ty, bool IsStore,
                                                 bool CPSOnly,
                                                 MDNode *TBAATag) {
  if (Ty->isFunctionTy() ||
      (Ty->isPointerTy() &&
       cast<PointerType>(Ty)->getElementType()->isFunctionTy())) {
    return true;

  } else if (Ty->isPrimitiveType() || Ty->isIntegerTy()) {
    return false;

  } else if (PointerType *PTy = dyn_cast<PointerType>(Ty)) {
    // FIXME: for unknown reason, clang sometimes generates function pointer
    // items in structs as {}* (e.g., in struct _citrus_iconv_ops). However,
    // clang keeps correct TBAA tags even in such cases, so we look at it first.
    if (IsStore && PTy->getElementType()->isStructTy() &&
        cast<StructType>(PTy->getElementType())->getNumElements() == 0 &&
        TBAATag && TBAATag->getNumOperands() > 1 &&
        cast<MDString>(TBAATag->getOperand(0))->getString() ==
            "function pointer") {
      return true;
    }

    if (CPSOnly) {
      Type *ElTy = PTy->getElementType();
      if (ElTy->isPointerTy()) {
      //     cast<PointerType>(ElTy)->getElementType()->isFunctionTy()) {
        // It could be a vtable pointer
        if (TBAATag) {
          assert(TBAATag->getNumOperands() > 1);
          MDString *TagName = cast<MDString>(TBAATag->getOperand(0));
          return TagName->getString() == "vtable pointer";
        }
      }

      if (IsStore && ElTy->isIntegerTy(8)) {
        // We want to instrument all stores of void* pointers, as those
        // might later be casted to protected pointers. Unfortunately,
        // LLVM represents all void* pointers as i8*, so we do something
        // very over-approximate here.

        if (TBAATag) {
          assert(TBAATag->getNumOperands() > 1);
          MDString *TagName = cast<MDString>(TBAATag->getOperand(0));
          return TagName->getString() == "void pointer" ||
                 TagName->getString() == "function pointer";
        }
      }

      return false;
    }

    if (IsStore && PTy->getElementType()->isIntegerTy(8)) {
      // We want to instrument all stores of void* pointers, as those
      // might later be casted to protected pointers. Unfortunately,
      // LLVM represents all void* pointers as i8*, so we do something
      // very over-approximate here.

      if (TBAATag) {
        assert(TBAATag->getNumOperands() > 1);
        MDString *TagName = cast<MDString>(TBAATag->getOperand(0));
        return TagName->getString() == "void pointer" ||
               TagName->getString() == "function pointer";
      }

      return true;
    }

    return shouldProtectType(PTy->getElementType(), IsStore, CPSOnly);

  } else if (SequentialType *PTy = dyn_cast<SequentialType>(Ty)) {
    return shouldProtectType(PTy->getElementType(), IsStore, CPSOnly);

  } else if (StructType *STy = dyn_cast<StructType>(Ty)) {
    if (STy->isOpaque())
      return IsStore;

    TypesProtectInfoTy::key_type Key(Ty, IsStore);
    TypesProtectInfoTy::iterator TIt = StructTypesProtectInfo.find(Key);
    if (TIt != StructTypesProtectInfo.end())
      return TIt->second;

    // Avoid potential infinite recursion due to recursive types
    // FIXME: support recursive types with sensitive members
    StructTypesProtectInfo[Key] = false;

    if (MDNode *UTBAATag = UnionsTBAA.lookup(STy)) {
      // This is a union, try casting it to all components
      for (unsigned i = 0, e = UTBAATag->getNumOperands(); i+1 < e; i += 2) {
        assert(isa<UndefValue>(UTBAATag->getOperand(i)));
        assert(isa<MDNode>(UTBAATag->getOperand(i+1)));

        Type *ElTy = UTBAATag->getOperand(i)->getType();
        MDNode *ElTBAATag = cast<MDNode>(UTBAATag->getOperand(i+1));
        if (shouldProtectType(ElTy, IsStore, CPSOnly, ElTBAATag)) {
          StructTypesProtectInfo[Key] = true;
          return true;
        }
      }

      return false;
    } else {
      // Tnis is not a union, go through all fields
      MDNode *STBAATag = StructsTBAA.lookup(STy);
      DEBUG(if (!STBAATag) {
        dbgs() << "CPI: missing struct TBAA for ";
        if (STy->hasName()) dbgs() << STy->getName();
        dbgs() << "\n    "; STy->dump();
        dbgs() << "\n";
      });

      const StructLayout *SL = STBAATag ? DL->getStructLayout(STy) : NULL;
      size_t STBAAIndex = 0;

      for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
        Type *ElTy = STy->getElementType(i);
        MDNode *ElTBAATag =
            getNextElTBAATag(STBAAIndex, ElTy, SL, i, STBAATag);

        if (shouldProtectType(ElTy, IsStore, CPSOnly, ElTBAATag)) {
          // Cache the results to speedup future queries
          StructTypesProtectInfo[Key] = true;
          return true;
        }
      }

      return false;
    }

  } else {
#ifndef NDEBUG
    Ty->dump();
#endif
    llvm_unreachable("Unhandled type");
  }
}

bool CPI::shouldProtectLoc(Value *Loc, bool IsStore) {
  if (!IsStore && AA->pointsToConstantMemory(Loc))
    return false; // Do not protect loads from constant memory

  SmallPtrSet<Value *, 8> Visited;
  SmallVector<Value *, 8> Worklist;
  Worklist.push_back(Loc);
  do {
    Value *P = Worklist.pop_back_val();
    P = GetUnderlyingObject(P, DL, 0);

    if (!Visited.insert(P))
      continue;

    if (SelectInst *SI = dyn_cast<SelectInst>(P)) {
      Worklist.push_back(SI->getTrueValue());
      Worklist.push_back(SI->getFalseValue());
      continue;
    }

    if (PHINode *PN = dyn_cast<PHINode>(P)) {
      for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i)
        Worklist.push_back(PN->getIncomingValue(i));
      continue;
    }

#if 0
    if (AllocaInst *AI = dyn_cast<AllocaInst>(P)) {
      if (!IsSafeStackAlloca(AI, DL)) {
        // Pointers on unsafe stack must be instrumented
        return true;
      }

      // Pointers on the safe stack can never be overwritten, no need to
      // instrument them.
      continue;
    }
#endif

    if (isa<GlobalVariable>(P) &&
        cast<GlobalVariable>(P)->isConstant()) {
      if (IsStore) {
        errs() << "CPI: a store to a constant?\n";
        return true; // Be conservative
      }

      // Constant globals never change, no need to instrument.

    } else {
      if (IsStore || !AA->pointsToConstantMemory(P))
        return true; // Stores or non-constant loads must be instrumented

      // Do not instrument constant loads
    }

  } while (!Worklist.empty());

  return false;
}

bool CPI::shouldProtectValue(Value *Val, bool IsStore,
                                                  bool CPSOnly,
                                                  MDNode *TBAATag,
                                                  Type *RealTy) {
  return shouldProtectType(RealTy ? RealTy : Val->getType(),
                           IsStore, CPSOnly, TBAATag);
}

static Constant *InsertInstructionLocStr(Instruction *I) {
  LLVMContext &C = I->getContext();
  Module &M = *I->getParent()->getParent()->getParent();
  std::string s;
  raw_string_ostream os(s);

  os << I->getParent()->getParent()->getName();

  DebugLoc DL = I->getDebugLoc();
  if (!DL.isUnknown()) {
    os << " (at " << DIScope(DL.getScope(C)).getFilename()
       << ":" << DL.getLine();
    if (DL.getCol()) os << ":" << DL.getCol();
    for (DebugLoc InlinedAtDL = DL;;) {
      InlinedAtDL = DebugLoc::getFromDILocation(InlinedAtDL.getInlinedAt(C));
      if (InlinedAtDL.isUnknown())
        break;
      os << ", inlined at ";
      os << DIScope(InlinedAtDL.getScope(C)).getFilename()
         << ":" << InlinedAtDL.getLine();
      if (InlinedAtDL.getCol()) os << ":" << InlinedAtDL.getCol();
    }
    os << ")";
  }

  Constant *Str = ConstantDataArray::getString(C, os.str());
  GlobalVariable *GV = new GlobalVariable(M, Str->getType(), true,
                                          GlobalValue::InternalLinkage,
                                          Str, "__llvm__cpi_debug_str");
  GV->setUnnamedAddr(true);
  return ConstantExpr::getPointerCast(GV, Type::getInt8PtrTy(M.getContext()));
}

bool CPI::doCPIInitialization(Module &M) {
  CreateCPIInterfaceFunctions(DL, M, IF);
  doStatsInitialization(M);
  return true;
}

bool CPI::mayBeCalledExternally(Function *F) {
  // FIXME: the following is only a heuristic...

  SmallSet<Value*, 16> Visited;
  SmallVector<Value*, 16> WorkList;
  WorkList.push_back(F);

  while (!WorkList.empty()) {
    Value *V = WorkList.pop_back_val();

    for (Value::use_iterator I = V->use_begin(),
                             E = V->use_end(); I != E; ++I) {
      User *U = *I;
      if (isa<BlockAddress>(U))
        continue;

      CallSite CS(U);
      if (CS) {
        if (CS.getCalledValue() != V && CS.getCalledFunction() &&
            CS.getCalledFunction()->isDeclaration())
          // May be passed to an external function
          return true;

        continue;
      }

      Operator *OP = dyn_cast<Operator>(U);
      if (OP) {
        switch (OP->getOpcode()) {
        case Instruction::BitCast:
        case Instruction::PHI:
        case Instruction::Select:
          if (Visited.insert(U))
            WorkList.push_back(U);
          break;
        default:
          break;
        }
      }
    }
  }

  return false;
}

#warning FIXME: this should take CPSOnly as an argument
void CPI::buildMetadataReload(
                IRBuilder<true, TargetFolder> &IRB, Value *VPtr,
                Value *EndPtr, BasicBlock *ExitBB, Value *PPt) {
  assert(VPtr->getType()->isPointerTy());

  Type *VTy = cast<PointerType>(VPtr->getType())->getElementType();

  if (isa<PointerType>(VTy)) {
    assert((cast<PointerType>(VTy)->getElementType()->isStructTy() &&
            cast<StructType>(cast<PointerType>(VTy)->getElementType())
              ->getNumElements() == 0)/* FIXME: requires TBAATag */
           || shouldProtectType(VTy, true, false));

    if (EndPtr) {
      // Check bounds
      assert(cast<PointerType>(EndPtr->getType())->getElementType()
             ->isIntegerTy(8));
      Value *Cond = IRB.CreateICmpULT(
            IRB.CreateBitCast(VPtr, IRB.getInt8PtrTy()),
            EndPtr); // XXX: include ptr size?
      ConstantInt *CCond = dyn_cast<ConstantInt>(Cond);
      if (CCond && CCond->isZero())
        return;

      if (!CCond) {
        Instruction *InsertPt = IRB.GetInsertPoint();
        assert(InsertPt);

        BasicBlock *PredBB = InsertPt->getParent();
        BasicBlock *NextBB = PredBB->splitBasicBlock(InsertPt);

        IRB.SetInsertPoint(PredBB, &PredBB->back());
        IRB.CreateCondBr(Cond, NextBB, ExitBB);
        PredBB->back().eraseFromParent();

        assert(InsertPt == NextBB->begin());
        IRB.SetInsertPoint(NextBB, NextBB->begin());
      } else {
        assert(CCond->isOne());
      }
    }

    if (PPt) incrementProfilePoint(IRB, PPt);

    IRB.CreateCall2(IF.CPISetFn,
        IRB.CreatePointerCast(VPtr, IRB.getInt8PtrTy()->getPointerTo()),
        IRB.CreatePointerCast(IRB.CreateLoad(VPtr), IRB.getInt8PtrTy()));

  } else if (ArrayType *STy = dyn_cast<ArrayType>(VTy)) {
    if (isa<CompositeType>(STy->getElementType())) {
      if (STy->getArrayNumElements() <= CPI_LOOP_UNROLL_TRESHOLD) {
        for (uint64_t i = 0, e = STy->getArrayNumElements(); i != e; ++i) {
          Value *Idx[2] = { IRB.getInt64(0), IRB.getInt64(i) };
          buildMetadataReload(IRB, IRB.CreateGEP(VPtr, Idx),
                              EndPtr, ExitBB, PPt);
        }
      } else {
          uint64_t Size = DL->getTypeStoreSize(STy);
          Value *Idx[2] = { IRB.getInt64(0), IRB.getInt64(0) };
          buildMetadataReloadLoop(IRB, IRB.CreateGEP(VPtr, Idx),
                                  IRB.getInt64(Size), PPt);
      }
    }

  } else if (VectorType *VecTy = dyn_cast<VectorType>(VTy)) {
    if (isa<CompositeType>(VecTy->getElementType())) {
      for (uint64_t i = 0, e = VecTy->getNumElements(); i != e; ++i) {
        Value *Idx[2] = { IRB.getInt64(0), IRB.getInt64(i) };
        buildMetadataReload(IRB, IRB.CreateGEP(VPtr, Idx), EndPtr, ExitBB, PPt);
      }
    }

  } else if (StructType *STy = dyn_cast<StructType>(VTy)) {
    if (MDNode *UTBAATag = UnionsTBAA.lookup(STy)) {
      // This is a union, try casting it to all components
      for (unsigned i = 0, e = UTBAATag->getNumOperands(); i+1 < e; i += 2) {
        assert(isa<UndefValue>(UTBAATag->getOperand(i)));
        assert(isa<MDNode>(UTBAATag->getOperand(i+1)));

        Type *ElTy = UTBAATag->getOperand(i)->getType();
        MDNode *ElTBAATag = cast<MDNode>(UTBAATag->getOperand(i+1));
        if (shouldProtectType(ElTy, true, false, ElTBAATag)) {
          buildMetadataReload(IRB,
                              IRB.CreateBitCast(VPtr, ElTy->getPointerTo()),
                              EndPtr, ExitBB, PPt);
          // FIXME: more than one field might contain metadata
          return;
        }
      }

    } else {
      MDNode *STBAATag = StructsTBAA.lookup(STy);
      const StructLayout *SL = STBAATag ? DL->getStructLayout(STy) : NULL;
      size_t STBAAIndex = 0;

      for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
        Type *ElTy = STy->getElementType(i);
        MDNode *ElTBAATag =
            getNextElTBAATag(STBAAIndex, ElTy, SL, i, STBAATag);
        if (shouldProtectType(ElTy, true, false, ElTBAATag)) {
          Value *Idx[2] = { IRB.getInt64(0), IRB.getInt32(i) };
          buildMetadataReload(IRB, IRB.CreateGEP(VPtr, Idx),
                              EndPtr, ExitBB, PPt);
        }
      }
    }
  }
}

void CPI::buildMetadataReloadLoop(
                IRBuilder<true, TargetFolder> &IRB, Value *VPtr, Value *Size,
                Value *PPt) {
  LLVMContext &C = IRB.getContext();
  assert(VPtr->getType()->isPointerTy());

  Type *ElTy = cast<PointerType>(VPtr->getType())->getElementType();
  assert(ElTy->isSized());

  if (ConstantInt *CSize = dyn_cast<ConstantInt>(Size)) {
    uint64_t CCSize = CSize->getZExtValue();
    uint64_t ElSize = DL->getTypeAllocSize(ElTy);

    uint64_t NumIter = (CCSize + ElSize - 1) / ElSize;
    if (NumIter <= CPI_LOOP_UNROLL_TRESHOLD) {
      BasicBlock *ExitBB = NULL;
      Value *EndPtr8 = NULL;
      if (NumIter * ElSize != CCSize) {
        EndPtr8 = IRB.CreateIntToPtr(
                   IRB.CreateAdd(IRB.CreatePtrToInt(VPtr, DL->getIntPtrType(C)),
                   Size), IRB.getInt8PtrTy());

        BasicBlock *ThisBB = IRB.GetInsertBlock();
        ExitBB = ThisBB->splitBasicBlock(IRB.GetInsertPoint());
        IRB.SetInsertPoint(ThisBB, ThisBB->getTerminator());
      }

      for (uint64_t i = 0; i != NumIter; ++i) {
        buildMetadataReload(IRB, IRB.CreateGEP(VPtr, IRB.getInt64(i)),
                            EndPtr8, ExitBB, PPt);
      }

      if (ExitBB) {
        IRB.SetInsertPoint(ExitBB, ExitBB->begin());
      }
      return;
    }
  }

  Value *EndPtr = IRB.CreateIntToPtr(
                   IRB.CreateAdd(IRB.CreatePtrToInt(VPtr, DL->getIntPtrType(C)),
                   Size), VPtr->getType());
  Value *EndPtr8 = IRB.CreateBitCast(EndPtr, IRB.getInt8PtrTy());

  Instruction *InsertPt = IRB.GetInsertPoint();
  BasicBlock *PreHeader = InsertPt->getParent();

  BasicBlock *Header = PreHeader->splitBasicBlock(InsertPt);
  BasicBlock *Body = Header->splitBasicBlock(InsertPt);
  BasicBlock *Exit = Body->splitBasicBlock(InsertPt);

  // Create loop condition
  IRB.SetInsertPoint(Header, Header->begin());

  PHINode *CurPtr = IRB.CreatePHI(VPtr->getType(), 2);
  CurPtr->addIncoming(VPtr, PreHeader);

  IRB.CreateCondBr(IRB.CreateICmpULT(CurPtr, EndPtr), Body, Exit);
  Header->back().eraseFromParent();

  // Create loop body
  IRB.SetInsertPoint(Body, Body->begin());
  buildMetadataReload(IRB, CurPtr, EndPtr8, Exit, PPt);

  Value *NextPtr = IRB.CreateGEP(CurPtr, IRB.getInt64(1));
  CurPtr->addIncoming(NextPtr, IRB.GetInsertBlock());

  IRB.CreateBr(Header);
  IRB.GetInsertBlock()->back().eraseFromParent();

  IRB.SetInsertPoint(Exit, Exit->begin());
}

static Type *guessRealValueType(Value *VPtr, Function *F) {
  // Look through the data flow upwards
#if 0
  for (Value *V1 = VPtr;; V1 = cast<Operator>(V1)->getOperand(0)) {
    if (PointerType *Ty = dyn_cast<PointerType>(V1->getType())) {
      if (!Ty->getElementType()->isIntegerTy(8) &&
          !Ty->getElementType()->isFunctionTy() &&
          Ty->getElementType()->isSized()) {
        return Ty->getElementType();
      }
    }

    // FIXME: support GEP
    if (!isa<Operator>(V1) ||
        cast<Operator>(V1)->getOpcode() != Instruction::BitCast)
      break;
  }
#endif
  Value *V1 = VPtr;
  do {
    PointerType *Ty = dyn_cast<PointerType>(V1->getType());
    if (!Ty)
      continue;

    Type *ElTy = Ty->getElementType();
    if (ElTy->isIntegerTy(8) || ElTy->isFunctionTy() || !ElTy->isSized())
      continue;

    if (ArrayType *ATy = dyn_cast<ArrayType>(ElTy)) {
      if (ATy->getElementType()->isIntegerTy(8) &&
          ATy->getNumElements() <= 1)
        continue; // [1 x i8] are often used as placeholders, ignore them
    }

    return ElTy;

    // FIXME: support GEP
  } while (isa<Operator>(V1) &&
           cast<Operator>(V1)->getOpcode() == Instruction::BitCast &&
           (V1 = cast<Operator>(V1)->getOperand(0)));


  // Look through the data flow downwards
  for (Value::use_iterator It = VPtr->use_begin(),
                           Ie = VPtr->use_end(); It != Ie; ++It) {
    User *U = *It;
    if (!isa<Operator>(U) ||
        cast<Operator>(U)->getOpcode() != Instruction::BitCast)
      continue;

    if (PointerType *Ty = dyn_cast<PointerType>(U->getType())) {
      if (!Ty->getElementType()->isIntegerTy(8) &&
          !Ty->getElementType()->isFunctionTy() &&
          Ty->getElementType()->isSized()) {
        /*
        dbgs() << "guessRealType: " << F->getName() << "\n";
        dbgs() << " op: "; VPtr->dump();
        dbgs() << " user: "; U->dump();
        dbgs() << "\n";
        */
        return Ty->getElementType();
      }
    }
  }

  return NULL;
}

static bool canBeVoidStar(Value *VPtr) {
  for (Value::use_iterator It = VPtr->use_begin(),
                           Ie = VPtr->use_end(); It != Ie; ++It) {
    IntrinsicInst *II = dyn_cast<IntrinsicInst>(*It);
    if (!II || II->getIntrinsicID() != Intrinsic::var_annotation)
      continue;

    StringRef Str;
    bool ok = getConstantStringInfo(II->getArgOperand(1), Str);
    assert(ok);

    if (Str != "tbaa.val")
      continue;

    MDNode *TBAATag = II->getMetadata("tbaa.val");
    assert(TBAATag != NULL);
    assert(TBAATag->getNumOperands() > 1);
    MDString *TagName = cast<MDString>(TBAATag->getOperand(0));
    return TagName->getString() == "void pointer" ||
           TagName->getString() == "function pointer";
  }

  // In the absence of annotations we must be conservative
  return true;
}

static bool canBeUniversalPtr(Value *VPtr) {
  // FIXME: re-add heuristics we had before
  Value *V1 = VPtr;
  do {
    if(PointerType *Ty = dyn_cast<PointerType>(V1->getType())) {
      if (ArrayType *ATy = dyn_cast<ArrayType>(Ty->getElementType())) {
        if (ATy->getElementType()->isIntegerTy(8) &&
            ATy->getNumElements() <= 1)
          return true; // [1 x i8] are often used as placeholders, ignore them
      }
    }
  } while (isa<Operator>(V1) &&
           cast<Operator>(V1)->getOpcode() == Instruction::BitCast &&
           (V1 = cast<Operator>(V1)->getOperand(0)));

  return canBeVoidStar(VPtr);
}

static bool isUsedAsFPtr(Value *FPtr) {
  // XXX: in povray spec benchmark, llvm creates spurious loads of
  // function pointers when it casts some classes into freelists.
  // We avoid this by checking whether the loaded value actually ends
  // up being used as a function pointer later on.

  SmallVector<Value*, 16> WorkList;
  WorkList.push_back(FPtr);

  while (!WorkList.empty()) {
    Value *Val = WorkList.pop_back_val();
    for (Value::use_iterator It = Val->use_begin(),
                             Ie = Val->use_end(); It != Ie; ++It) {
      User *U = *It;
      if (CastInst *CI = dyn_cast<CastInst>(U)) {
        if (PointerType *PTy = dyn_cast<PointerType>(CI->getType()))
          if (PTy->getElementType()->isFunctionTy())
            return true; // cast to another function pointer type
      } else if (isa<CmpInst>(U)) {
        continue;
      } else if (isa<PHINode>(U) || isa<SelectInst>(U)) {
        WorkList.push_back(U);
      } else {
        // Any non-cast instruction
        return true;
      }
    }
  }

  // FPtr is only used in cast insts to non-function-pointer types
  return false;
}

bool CPI::isUsedInProtectContext(Value *Ptr, bool
                                                      CPSOnly) {

  SmallPtrSet<Value*, 16> Visited;
  SmallVector<Value*, 16> WorkList;
  Visited.insert(Ptr);
  WorkList.push_back(Ptr);

  while (!WorkList.empty()) {
    Value *Val = WorkList.pop_back_val();
    for (Value::use_iterator It = Val->use_begin(),
                             Ie = Val->use_end(); It != Ie; ++It) {
      User *U = *It;
      if (CastInst *CI = dyn_cast<CastInst>(U)) {
        if (PointerType *PTy = dyn_cast<PointerType>(CI->getType()))
          if (shouldProtectType(PTy, false, CPSOnly))
            return true; // cast to another function pointer type
      } else if (isa<CmpInst>(U)) {
        continue;
      } else if (isa<PHINode>(U) || isa<SelectInst>(U)) {
        if (Visited.insert(U))
          WorkList.push_back(U);
      } else {
        // Any non-cast instruction
        return true;
      }
    }
  }

  return false;
}

bool CPI::pointsToVTable(Value *Ptr) {
  SmallVector<Value*, 8> Objects;
  GetUnderlyingObjects(Ptr, Objects, DL);
  for (unsigned i = 0, e = Objects.size(); i != e; ++i) {
    Instruction *I = dyn_cast<Instruction>(Objects[i]);
    if (!I)
      return false;

    MDNode *TBAATag = I->getMetadata(LLVMContext::MD_tbaa);
    if (!TBAATag)
      return false;

    StringRef T = cast<MDString>(TBAATag->getOperand(0))->getString();
    if (T != "vtable pointer")
      return false;
  }

  return true;
}

Value *CPI::createBounds(IRBuilder<> &IRB,
                                              Value *Base, Value *Size) {
  Base = IRB.CreatePtrToInt(Base, IntPtrTy);
  Value *Last = IRB.CreateSub(
    IRB.CreateAdd(Base, IRB.CreateIntCast(Size, IntPtrTy, false)),
    ConstantInt::get(IntPtrTy, 1));
  Value *R = UndefValue::get(BoundsTy);
  R = IRB.CreateInsertElement(R, Base, IRB.getInt32(0));
  R = IRB.CreateInsertElement(R, Last, IRB.getInt32(1));
  return R;
}

Value *CPI::insertBoundsAndChecks(
        Value *V, DenseMap<Value*, Value*> &BM,
        SetVector<Value*> &NeedBounds,
        std::vector<std::pair<Instruction*,
                              std::pair<Value*, uint64_t> > > &BoundsChecks,
        SmallPtrSet<Value*, 64> &IsDereferenced,
        SetVector<std::pair<Instruction*, Instruction*> > &ReplMap) {
  Value *Result = NULL;

  if (Value *B = BM.lookup(V)) {
    // V was already visited before
    Result = B;
  } else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
    // Check whether we also need to check the load address
    if (NeedBounds.count(LI->getPointerOperand()) == 0) {
      // TODO: metadata
      // TODO: do we really need the following extra check ?
      // TODO: vaarg
      if (!isa<Constant>(LI->getPointerOperand()) &&
          shouldProtectType(LI->getPointerOperand()->getType(), false, true)) {
        NeedBounds.insert(LI->getPointerOperand());
        IsDereferenced.insert(LI->getPointerOperand());
        BoundsChecks.push_back(std::make_pair(LI,
                        std::make_pair(LI->getPointerOperand(),
                                       DL->getTypeStoreSize(LI->getType()))));
      }
    }

    if (LI->getType()->isPointerTy() &&
        !LI->getMetadata("vaarg.load") &&
        shouldProtectLoc(LI->getPointerOperand(), false) &&
        shouldProtectValue(LI, /* IsStore= */ false, /* CPSOnly = */ false,
                           LI->getMetadata(LLVMContext::MD_tbaa)) &&
        !pointsToVTable(LI->getPointerOperand()) &&
        isUsedInProtectContext(V, false)) {

      ++NumProtectedLoads;

      // Load bounds from memory
      IRBuilder<> IRB(LI->getNextNode());
      IRB.SetCurrentDebugLocation(LI->getDebugLoc());

      bool IsValDereferenced = IsDereferenced.count(V);
      insertProfilePoint(IRB, LI,
          (!CPIDebugMode && IsValDereferenced) ? "load-nocheck" : "load-full");

      if (!CPIDebugMode) {
        Value *MD = IRB.CreateCall(
              IsValDereferenced ? IF.CPIGetMetadataNocheckFn :
                                  IF.CPIGetMetadataFn,
              IRB.CreatePointerCast(LI->getPointerOperand(),
                                    IRB.getInt8PtrTy()->getPointerTo()));
        Result = IRB.CreateCall(IF.CPIGetBoundsFn, MD);
        Instruction *SVal = IRB.CreateCall(IF.CPIGetValFn, MD);
        if (MDNode *TBAA = LI->getMetadata(LLVMContext::MD_tbaa))
          SVal->setMetadata(LLVMContext::MD_tbaa, TBAA);
        SVal = cast<Instruction>(IRB.CreateBitCast(SVal, LI->getType()));

        bool inserted = ReplMap.insert(std::make_pair(LI, SVal));
        assert(inserted);
        //LI->replaceAllUsesWith(SVal);

        BM[SVal] = NULL; // Make sure we won't try to re-instrument this

        // We do not erase the load here to not invalidate our maps.
        // Instead, we trust DCE to erase the load later on.
      } else {
        Result = IRB.CreateCall3(IF.CPIAssertFn,
                  IRB.CreatePointerCast(LI->getPointerOperand(),
                                        IRB.getInt8PtrTy()->getPointerTo()),
                  IRB.CreatePointerCast(LI, IRB.getInt8PtrTy()),
                  InsertInstructionLocStr(LI));
      }

    } else {
      // XXX: need to think through on what to do in this case
      Result = InftyBounds;
    }

  } else if (Argument *A = dyn_cast<Argument>(V)) {
    if (MayBeCalledExternally.lookup(A->getParent())) {
      // XXX: this function may be called externally, cannot rely on our
      // modified calling convention
      Result = InftyBounds;
    } else if (!shouldProtectValue(A, true, false)) {
      // XXX: if shouldProtectValue returns false here, it means the function
      // call will not be instrumented either. Need to think about this.
      Result = InftyBounds;
    } else {
      IRBuilder<> IRB(A->getParent()->getEntryBlock().getFirstInsertionPt());
      Result = IRB.CreateCall(IF.CPIGetArgBoundsFn,
                              IRB.getInt32(1 + A->getArgNo()));
    }

  } else if (isa<CallInst>(V) || isa<InvokeInst>(V)) {
    CallSite CS(cast<Instruction>(V));
    Function *CF = CS.getCalledFunction();

    IRBuilder<> IRB(isa<CallInst>(V) ? cast<Instruction>(V)->getNextNode() :
                            (Instruction*) (cast<InvokeInst>(V)->getNormalDest()
                                            ->getFirstInsertionPt()));
    IRB.SetCurrentDebugLocation(cast<Instruction>(V)->getDebugLoc());

    if (isMallocLikeFn(CS.getCalledValue(), TLI, true)) {
      Result = createBounds(IRB, V, CS.getArgument(0));
    } else if (isReallocLikeFn(CS.getCalledValue(), TLI, true)) {
      Result = createBounds(IRB, V, CS.getArgument(1));
    } else if (isCallocLikeFn(CS.getCalledValue(), TLI, true)) {
      Result = createBounds(IRB, V, IRB.CreateMul(CS.getArgument(0),
                                                  CS.getArgument(1)));
    } else if (!CF || (CF && (CF->isDeclaration() ||
                              CF->getName().startswith("llvm.") ||
                              CF->getName().startswith("__llvm__")))) {
      // XXX: we assume infty bounds on unknown functions
      Result = InftyBounds;
    } else {
      if (shouldProtectType(CF->getReturnType(), true, false)) {
        bool CriticalEdge = false;
        if (InvokeInst *II = dyn_cast<InvokeInst>(V)) {
          PHINode *PN;
          for (BasicBlock::iterator It = II->getNormalDest()->begin();
                (PN = dyn_cast<PHINode>(It)) && !CriticalEdge; ++It) {
            for (PHINode::value_op_iterator VIt = PN->value_op_begin(),
                                VIe = PN->value_op_end(); VIt != VIe; ++VIt) {
              if (*VIt == II) {
                CriticalEdge = true;
                break;
              }
            }
          }
#if 0 // This is what we should do if we're no the critical edge.
      // But it crashes :-(
          // We should avoid cases where the result of InvokeInst is used
          // in the successor block in a PHINode
          BasicBlock *NewBB = SplitCriticalEdge(II->getParent(),
                                                II->getNormalDest(), this);
          if (NewBB)
            IRB.SetInsertPoint(NewBB, NewBB->getFirstInsertionPt());
#endif
        }
        if (CriticalEdge) {
          Result = InftyBounds; // XXX!
        } else {
          // Load bounds from return buffer
          insertProfilePoint(IRB, CS.getInstruction(), "bounds-arg-load");
          Result = IRB.CreateCall(IF.CPIGetArgBoundsFn, IRB.getInt32(0));
        }
      } else {
      // XXX: if shouldProtectValue returns false here, it means the function
      // return will not be instrumented either. Need to think about this.
        Result = InftyBounds;
      }
    }

  } else if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
    IRBuilder<> IRB(AI->getNextNode());
    IRB.SetCurrentDebugLocation(AI->getDebugLoc());
    Value *Size = IRB.CreateMul(
      IRB.CreateIntCast(AI->getArraySize(), IntPtrTy, false),
      ConstantInt::get(IntPtrTy, DL->getTypeAllocSize(AI->getAllocatedType())));
    Result = createBounds(IRB, AI, Size);

  } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
    IRBuilder<> IRB(GV->getContext());
    if (GV->getType()->getElementType()->isSized()) {
      Result = createBounds(IRB, GV, ConstantInt::get(IntPtrTy,
                      DL->getTypeAllocSize(GV->getType()->getElementType())));
    } else {
      // FIXME: we should use GetElementPtr instruction here! That way, it
      // will resolve correctly as soon as the type becames non-opaque
      Result = InftyBounds;
    }

  } else if (GlobalAlias *GA = dyn_cast<GlobalAlias>(V)) {
    Result = insertBoundsAndChecks(GA->getAliasee(), BM, NeedBounds,
                                   BoundsChecks, IsDereferenced, ReplMap);

  } else if (isa<Function>(V)) {
    // Function values should never be used as address except for CallSite
    Result = EmptyBounds;

  } else if (PHINode *PHI = dyn_cast<PHINode>(V)) {
    unsigned N = PHI->getNumIncomingValues();
    IRBuilder<> IRB(PHI->getNextNode());
    IRB.SetCurrentDebugLocation(PHI->getDebugLoc());

    PHINode *NewPHI = IRB.CreatePHI(BoundsTy, N);
    BM[V] = NewPHI; // Add early, to avoid infinite recursion on loops

    for (unsigned i = 0; i < N; ++i)
      NewPHI->addIncoming(insertBoundsAndChecks(PHI->getIncomingValue(i), BM,
                                                NeedBounds, BoundsChecks,
                                                IsDereferenced, ReplMap),
                          PHI->getIncomingBlock(i));

    Result = NewPHI;

  } else if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
    IRBuilder<> IRB(SI->getNextNode());
    IRB.SetCurrentDebugLocation(SI->getDebugLoc());
    Result = IRB.CreateSelect(SI->getCondition(),
      insertBoundsAndChecks(SI->getTrueValue(), BM, NeedBounds,
                            BoundsChecks, IsDereferenced, ReplMap),
      insertBoundsAndChecks(SI->getFalseValue(), BM, NeedBounds,
                            BoundsChecks, IsDereferenced, ReplMap));

  } else if (isa<ExtractValueInst>(V)) {
    Result = InftyBounds; // XXX FIXME

  } else if (isa<InsertValueInst>(V)) {
    Result = InftyBounds; // XXX FIXME

  } else if (isa<InlineAsm>(V)) {
    Result = InftyBounds;

  } else if (Operator *OP = dyn_cast<Operator>(V)) {
    switch (OP->getOpcode()) {
    case Instruction::BitCast:
    case Instruction::GetElementPtr:
      Result = insertBoundsAndChecks(OP->getOperand(0), BM, NeedBounds,
                                     BoundsChecks, IsDereferenced, ReplMap);
      break;

    case Instruction::IntToPtr:
    case Instruction::PtrToInt:
      // Try to follow the int, perhaps it's be casted back to a pointer soon.
      Result = insertBoundsAndChecks(OP->getOperand(0), BM, NeedBounds,
                                     BoundsChecks, IsDereferenced, ReplMap);
      break;

    case Instruction::Sub:
    case Instruction::Add:
    case Instruction::Mul:
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::URem:
    case Instruction::SRem:
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Shl:
    case Instruction::AShr:
    case Instruction::LShr:
      Result = EmptyBounds;
      break;

    case Instruction::SExt:
    case Instruction::ZExt:
      Result = EmptyBounds;
      break;

    case Instruction::AtomicCmpXchg:
      Result = InftyBounds; // FIXME: support this
      break;

    case Instruction::AtomicRMW:
      Result = EmptyBounds; // FIXME: what about xchg here?
      if (AtomicRMWInst *AI = dyn_cast<AtomicRMWInst>(OP)) {
        if (AI->getOperation() == AtomicRMWInst::Xchg)
          Result = InftyBounds; // FIXME:
      }
      break;

    default:
      break;
    }

  } else if (Constant *C = dyn_cast<Constant>(V)) {
    if (C->isNullValue() || isa<ConstantInt>(V) || isa<UndefValue>(V))
      Result = EmptyBounds;
    else
      Result = InftyBounds; // XXX
  }

  if (Result)
    return BM[V] = Result;
  //else // XXX: the following is a huge hack
  //  return BM[V] = InftyBounds;

#ifndef NDEBUG
  V->dump();
#endif
  llvm_unreachable("Unsupported bounds operation");
}

void CPI::insertChecks(DenseMap<Value*, Value*> &BM,
        Value *V, bool IsDereferenced,
        SetVector<std::pair<Instruction*, Instruction*> > &ReplMap) {
  if (BM.count(V)) {
    return; // Already visited
  }

  BM[V] = NULL;
  if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
    // Check whether our load is of instrumentable type
    // and is from instrumentable location
    if (LI->getType()->isPointerTy() &&
        !LI->getMetadata("vaarg.load") &&
        shouldProtectLoc(LI->getPointerOperand(), false) &&
        shouldProtectValue(LI, /* IsStore= */ false, /* CPSOnly = */ true,
                           LI->getMetadata(LLVMContext::MD_tbaa)) &&
        !pointsToVTable(LI->getPointerOperand()) &&
        isUsedAsFPtr(V)) {

      ++NumProtectedLoads;
      IRBuilder<> IRB(LI->getNextNode());
      IRB.SetCurrentDebugLocation(LI->getDebugLoc());
      insertProfilePoint(IRB, LI,
              (!CPIDebugMode && IsDereferenced) ? "load-nocheck" : "load-full");

      if (!CPIDebugMode) {
        if (SoftwareMode) {
          Value *MD = IRB.CreateCall(
            IsDereferenced ? IF.CPIGetMetadataNocheckFn :
            IF.CPIGetMetadataFn,
            IRB.CreatePointerCast(LI->getPointerOperand(),
                                  IRB.getInt8PtrTy()->getPointerTo()));
          Instruction *SVal = IRB.CreateCall(IF.CPIGetValFn, MD);
          if (MDNode *TBAA = LI->getMetadata(LLVMContext::MD_tbaa))
            SVal->setMetadata(LLVMContext::MD_tbaa, TBAA);
          SVal = cast<Instruction>(IRB.CreateBitCast(SVal, LI->getType()));

          bool inserted = ReplMap.insert(std::make_pair(LI, SVal));
          assert(inserted);

          //LI->replaceAllUsesWith(SVal);
          BM[SVal] = NULL; // Make sure we won't try to re-instrument this
        } else {
          decorateInstruction(LI, LI->getContext());
        }
        // We do not erase the load here to not invalidate our maps.
        // Instead, we trust DCE to erase the load later on.

      } else {
        IRB.CreateCall3(IF.CPIAssertFn,
              IRB.CreatePointerCast(LI->getPointerOperand(),
                                    IRB.getInt8PtrTy()->getPointerTo()),
              IRB.CreatePointerCast(LI, IRB.getInt8PtrTy()),
              InsertInstructionLocStr(LI));
      }
    }

  } else if (isa<CallInst>(V) || isa<InvokeInst>(V) || isa<Argument>(V) ||
             isa<AllocaInst>(V) || isa<Constant>(V)) {
    // Do nothing
  } else if (PHINode *PHI = dyn_cast<PHINode>(V)) {
    unsigned N = PHI->getNumIncomingValues();
    for (unsigned i = 0; i < N; ++i)
      insertChecks(BM, PHI->getIncomingValue(i), IsDereferenced, ReplMap);
  } else if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
    insertChecks(BM, SI->getTrueValue(), IsDereferenced, ReplMap);
    insertChecks(BM, SI->getFalseValue(), IsDereferenced, ReplMap);
  } else if (BitCastInst *CI = dyn_cast<BitCastInst>(V)) {
    insertChecks(BM, CI->getOperand(0), IsDereferenced, ReplMap);
  } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    insertChecks(BM, GEP->getPointerOperand(), IsDereferenced, ReplMap);
  } else if (isa<IntToPtrInst>(V)) {
    // XXX: this happens when the program contains unions with ints and fptrs.
    // When program stores ints to the union, LLVM sometimes transformes it
    // into casting an int to fptr and then storing the fptr. We should fix it
    // by either adding metadata or disabling such transformations. For now,
    // let's silently allow it.

  } else if (isa<InlineAsm>(V)) {
    // XXX: we can't do much about inline asm. Perhaps we should warn the user ?

  } else {
#ifndef NDEBUG
    //V->dump();
#endif
    //llvm_unreachable("Unsupported bounds operation");
  }
}

static bool isVariableSizedStruct(Type *Ty) {
  if (StructType *STy = dyn_cast<StructType>(Ty)) {
    if (STy->isOpaque())
      return false;

    Type *LastElTy = STy->getElementType(STy->getNumElements()-1);
    if (ArrayType *ATy = dyn_cast<ArrayType>(LastElTy)) {
      if (ATy->getArrayNumElements() == 0)
        return true;
    }
  }
  return false;
}

bool CPI::runOnFunction(Function &F) {
  LLVMContext &C = F.getContext();

  bool CPSOnly;
  if (F.hasFnAttribute("cpi")) {
    assert(!F.hasFnAttribute("cps"));
    F.addFnAttr("has-cpi");
    CPSOnly = false;
    HasCPIFullFunctions = true;
  } else if (F.hasFnAttribute("cps")) {
    assert(!F.hasFnAttribute("cpi"));
    F.addFnAttr("has-cps");
    CPSOnly = true;
  } else {
    return false;
  }

  {
    AttrBuilder B; B.addAttribute("cps").addAttribute("cpi");
    F.removeAttributes(AttributeSet::FunctionIndex,
        AttributeSet::get(C, AttributeSet::FunctionIndex, B));
  }

  Type *Int8PtrTy = Type::getInt8PtrTy(C);
  Type *Int8PtrPtrTy = Int8PtrTy->getPointerTo();

  // A list of all values that require bounds information
  SetVector<Value*> NeedBounds;

  // Store each value from NeedBounds that is dereferenced in the code.
  // For such values, the metadata get code might be simplified by allowing it
  // to crash when the metadata is absent or null.
  SmallPtrSet<Value*, 64> IsDereferenced;

  // A list of (insert point, loc, var) of all places where bounds
  // should be stored.
  std::vector<std::pair<Instruction*,
      std::pair<Value*, Value*> > > BoundsSTabStores;

  // A listof (insert point, idx, var) of all places where argument bounds
  // should be stored. Empty for CPSOnly.
  std::vector<std::pair<Instruction*,
      std::pair<unsigned, Value*> > > BoundsArgsStores;

  // A list of (insert point, var, size) of all places where bounds checks
  // should be inserted. Empty for CPSOnly.
  std::vector<std::pair<Instruction*,
      std::pair<Value*, uint64_t> > > BoundsChecks;

  // Collect all values that require bounds information
  for (inst_iterator It = inst_begin(F), Ie = inst_end(F); It != Ie; ++It) {
    Instruction *I = &*It;

    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
      ++NumLoads;
      // On load, we do NOT check whether to protect the loaded value or not.
      // Instead, we will protect it only if used in a context that requires
      // protection. E.g. imagine we do p->q->i = 0. Even if p and p->q requires
      // protection, we will only insert it if p->q->i also needs it.

      // However, in CPS mode, we do check the vtable ptr load
      // instructions and instrument them.
      if (CPSOnly) {
        MDNode *TBAATag = LI->getMetadata(LLVMContext::MD_tbaa);
        if (TBAATag &&
            cast<MDString>(TBAATag->getOperand(0))
              ->getString() == "vtable pointer") {
          NeedBounds.insert(LI);
          IsDereferenced.insert(LI);
        }
      }

#if 0
#warning support aggregate loads
      if (LI->getType()->isPointerTy() &&
          shouldProtectValue(LI, false, CPSOnly,
                             LI->getMetadata(LLVMContext::MD_tbaa))) {
        // If so, add bounds check to the load address
        NeedBounds.push_back(LI->getPointerOperand());

        if (!CPSOnly) {
          BoundsChecks.push_back(std::make_pair(LI,
                          std::make_pair(LI->getPointerOperand(),
                                         DL->getTypeStoreSize(LI->getType()))));
        }

        assert(!CPSOnly ||
               (LI->getMetadata(LLVMContext::MD_tbaa) &&
                cast<MDString>(
                  LI->getMetadata(LLVMContext::MD_tbaa)->getOperand(0)
                )->getString() == "vtable pointer"));

      }
#endif
    } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
      ++NumStores;
      // If we store a protected value, then we need to make sure the store
      // address is protected, and also store the protection information
      // for the store value.
#warning support aggregate stores
      if (SI->getValueOperand()->getType()->isPointerTy() &&
          (shouldProtectValue(SI->getValueOperand(), true, CPSOnly,
                              SI->getMetadata(LLVMContext::MD_tbaa)) ||
           // XXX: the optimizer sometimes lifts the bitcast out of the store
           (isa<Operator>(SI->getValueOperand()) &&
            cast<Operator>(SI->getValueOperand())->getOpcode() ==
              Instruction::BitCast &&
            shouldProtectValue(
              cast<Operator>(SI->getValueOperand())->getOperand(0),
              true, CPSOnly)))) {
        if (!CPSOnly && !isa<Constant>(SI->getPointerOperand())) {
          // Add bounds check to the store address
          NeedBounds.insert(SI->getPointerOperand());
          IsDereferenced.insert(SI->getPointerOperand());
          BoundsChecks.push_back(std::make_pair(SI,
              std::make_pair(SI->getPointerOperand(),
                   DL->getTypeStoreSize(SI->getValueOperand()->getType()))));
        }

        // Store bounds information for the stored value
        NeedBounds.insert(SI->getValueOperand());
        BoundsSTabStores.push_back(std::make_pair(SI,
            std::make_pair(SI->getPointerOperand(), SI->getValueOperand())));
      }
    } else if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
      // If we call through a pointer, check the pointer
      CallSite CS(I);
      if (!isa<Constant>(CS.getCalledValue())) {
        NeedBounds.insert(CS.getCalledValue());
        IsDereferenced.insert(CS.getCalledValue());
      }

      Function *CF = CS.getCalledFunction();

      if (CF && (CF->getName().startswith("llvm.") ||
                 CF->getName().startswith("__llvm__")))
        continue;

      ++NumCalls;
      if (!isa<Constant>(CS.getCalledValue()))
        ++NumIndirectCalls;

      for (unsigned i = 0, e = CS.arg_size(); i != e; ++i) {
        Value *A = CS.getArgument(i);
        if (shouldProtectValue(A, true, CPSOnly)) {
          // If we pass a value that needs protection as an arg, check it
          if (!CPSOnly || !isa<Constant>(A))
            NeedBounds.insert(A);
          if (!CPSOnly) {
            // Pass the bounds to the callee in !CPSOnly mode
            BoundsArgsStores.push_back(std::make_pair(I,
                                          std::make_pair(1+i, A)));
          }
        }
      }
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
      Value *RV = RI->getReturnValue();
      if (RV && !isa<Constant>(RV) && shouldProtectValue(RV, true, CPSOnly)) {
        // If we return a value that needs protectoin, check it
        NeedBounds.insert(RV);
        if (!CPSOnly) {
          // Pass the bounds to the caller in !CPSOnly mode
          BoundsArgsStores.push_back(std::make_pair(I,
                                        std::make_pair(0, RV)));
        }
      }
    }
  }

  // Cache bounds information for every value in the function
  DenseMap<Value*, Value*> BoundsMap;
  SetVector<std::pair<Instruction*, Instruction*> > ReplMap;

  if (CPSOnly) {
    // Insert load checks along the way, using BoundsMap as a visited set
    for (unsigned i = 0, e = NeedBounds.size(); i != e; ++i)
      insertChecks(BoundsMap, NeedBounds[i],
                   IsDereferenced.count(NeedBounds[i]), ReplMap);
  } else {
    // Populate BoundsMap and insert load checks along the way.
    // The insertBoundsAndChecks function might add new requests to the
    // NeedBounds and BoundsChecks arrays.
    for (unsigned i = 0; i < NeedBounds.size(); ++i)
      insertBoundsAndChecks(NeedBounds[i], BoundsMap, NeedBounds,
                            BoundsChecks, IsDereferenced, ReplMap);
  }

  // Add stab values and bounds stores
  for (unsigned i = 0, e = BoundsSTabStores.size(); i != e; ++i) {
    IRBuilder<> IRB(BoundsSTabStores[i].first);
    insertProfilePoint(IRB, BoundsSTabStores[i].first, "store");

    Value *Loc = IRB.CreateBitCast(BoundsSTabStores[i].second.first,
                                   Int8PtrPtrTy);
    Value *Val = IRB.CreateBitCast(BoundsSTabStores[i].second.second,
                                   Int8PtrTy);

    ++NumProtectedStores;
    if (CPSOnly) {
      if (SoftwareMode) {
        IRB.CreateCall2(IF.CPISetFn, Loc, Val);
      } else {
        decorateInstruction(BoundsSTabStores[i].first, C);
      }
    } else {
      Value *Bounds = BoundsMap.lookup(BoundsSTabStores[i].second.second);
      assert(Bounds && "Bounds insertion faliled?");
      IRB.CreateCall3(IF.CPISetBoundsFn, Loc, Val, Bounds);
    }
  }

  // Add args bounds stores
  assert(!CPSOnly || BoundsArgsStores.empty());
  for (unsigned i = 0, e = BoundsArgsStores.size(); i != e; ++i) {
    ++NumBoundsChecks;

    IRBuilder<> IRB(BoundsArgsStores[i].first);
    insertProfilePoint(IRB, BoundsArgsStores[i].first, "bounds-arg-store");
    Value *Bounds = BoundsMap.lookup(BoundsArgsStores[i].second.second);
    assert(Bounds && "Bounds insertion faliled?");

    IRB.CreateCall2(IF.CPISetArgBoundsFn,
                    IRB.getInt32(BoundsArgsStores[i].second.first), Bounds);
  }

  // Add bounds checks
#warning TODO: eliminate unnessesary checks (dominated by, from const mem, etc.)
#warning TODO: move checks out of the loops
  assert(!CPSOnly || BoundsChecks.empty());
  for (unsigned i = 0, e = BoundsChecks.size(); i != e; ++i) {
    IRBuilder<> IRB(BoundsChecks[i].first);
    insertProfilePoint(IRB, BoundsChecks[i].first, "bounds-check");

    Value *Val = IRB.CreateBitCast(BoundsChecks[i].second.first, Int8PtrTy);
    Value *Bounds = BoundsMap.lookup(BoundsChecks[i].second.first);
    Value *Size = ConstantInt::get(IntPtrTy, BoundsChecks[i].second.second);
    assert(Bounds && "Bounds insertion faliled?");

    IRB.CreateCall4(IF.CPIAssertBoundsFn, Val, Size, Bounds,
                    InsertInstructionLocStr(BoundsChecks[i].first));
  }

  // Instrument memory manipulating intrinsics
  for (inst_iterator It = inst_begin(F), Ie = inst_end(F); It != Ie;) {
    Instruction *I = &*(It++);
    CallInst *CI = dyn_cast<CallInst>(I);
    if (!CI)
      continue;

    Function *CF = CI->getCalledFunction();
    StringRef N = CF ? CF->getName() : StringRef();

    bool IsBZero = N.equals("bzero");
    bool IsMemSet = !IsBZero && (N.startswith("llvm.memset") ||
        N.equals("memset") || N.equals("__memset") ||
        N.equals("memset_chk") || N.equals("__memset_chk")); //||
        //N.startswith("__inline_memset"));
    bool IsMemCpy = !IsBZero && !IsMemSet && (N.startswith("llvm.memcpy") ||
        N.equals("memcpy") || N.equals("__memcpy") ||
        N.equals("memcpy_chk") || N.equals("__memcpy_chk")); // ||
        //N.startswith("__inline_memcpy"));
    bool IsMemMove = !IsBZero && !IsMemSet && !IsMemCpy &&
        (N.startswith("llvm.memmove") ||
        N.equals("memmove") || N.equals("__memmove") ||
        N.equals("memmove_chk") || N.equals("__memmove_chk")); // ||
        //N.startswith("__inline_memmove"));
    bool IsBCopy = !IsBZero && !IsMemSet && !IsMemCpy && !IsMemMove &&
        N.equals("bcopy");

    if (IsBZero || IsMemSet || IsMemCpy || IsMemMove || IsBCopy) {
      //assert(CF->isDeclaration());
      if (!(!IsBZero || CI->getNumArgOperands() == 2))
        continue;
      if (!(IsBZero ||
             (N.startswith("llvm.") && CI->getNumArgOperands() == 5) ||
             (N.endswith("_chk") && CI->getNumArgOperands() == 4) ||
             (CI->getNumArgOperands() == 3)))
        continue;

      ++NumMemcpyLikeOps;

      // We try to move through bitcasts and hope to succeed.
      Value *DstOp = CI->getArgOperand(IsBCopy ? 1 : 0);
      Value *SrcOp = (IsBZero || IsMemSet) ? NULL :
                      CI->getArgOperand(IsBCopy ? 0 : 1);
      assert(DstOp->getType()->isPointerTy());
      Type *RealTy = guessRealValueType(DstOp, &F);
      Type *RealTy2 = SrcOp ? guessRealValueType(SrcOp, &F) : NULL;
      bool Force = false;

      if (!RealTy && RealTy2) {
          RealTy = RealTy2; RealTy2 = NULL;
      }

      if (RealTy && !CPSOnly && IsMemSet) {
        if (CI->getMetadata("tbaa.items")) {
          if (ConstantInt *CSize =
              dyn_cast<ConstantInt>(CI->getArgOperand(2))) {
            if (DL->getTypeStoreSize(RealTy) != CSize->getZExtValue()) {
              // C++ often casts first class member to void* and passes it to
              // memset that initializes multiple members at a time. We rely
              // on tbaa.items in such cases
              RealTy = NULL;
            }
          }
        }
      }

      if (RealTy && RealTy->isPointerTy() &&
          cast<PointerType>(RealTy)->getElementType()->isIntegerTy(8) &&
          canBeUniversalPtr(DstOp)) {
        RealTy = RealTy2 = NULL;
      }


      if (RealTy && isVariableSizedStruct(RealTy)) {
        RealTy = RealTy2 = NULL;
        Force = true;
      }


      if (!RealTy && !RealTy2) {

        // We can't get any useful type info, just i8* which is either
        // void* or char*. Let's try some heuristics, then do a general
        // reset or copy operation.

        do {
          if (!Force) {
            if (IsMemSet) {
              Constant *CVal = dyn_cast<Constant>(CI->getArgOperand(1));
              if (!CVal || !CVal->isNullValue()) {
                // Do not instrument memset with non-zero value. It is unlikely
                // that a pointer is initialized to non-zero value using memset.
                break;
              }
            }

            if (!canBeUniversalPtr(DstOp))
              break; // The value is not void*

            if (IsMemCpy || IsMemMove || IsBCopy) {
              if (!canBeUniversalPtr(SrcOp))
                break; // The value is not void*
            }

            if (MDNode *Tags = CI->getMetadata("tbaa.items")) {
              bool HasPointers = false;
              for (unsigned i = 0, e = Tags->getNumOperands(); i != e; ++i) {
                if (!Tags->getOperand(i)) {
                  // XXX: this happens sometimes for pointer types
                  HasPointers = true;
                  break;
                  //continue;
                }
                StringRef Tag = cast<MDString>(
                      cast<MDNode>(Tags->getOperand(i))->getOperand(0))
                    ->getString();
                if (Tag == "void pointer" || Tag == "function pointer"
                    || (!CPSOnly && Tag == "any non-void pointer")) {
                  HasPointers = true;
                  break;
                }
              }
              // FIXME: use pointers layout instead of just HasPointers flag
              if (!HasPointers)
                break;
            }
          }

          // Looks like we need to instrument this op
          ++NumProtectedMemcpyLikeOps;

          IRBuilder<> IRB(CI->getNextNode());
          IRB.SetCurrentDebugLocation(CI->getDebugLoc());

          Type *Int8PtrTy = IRB.getInt8PtrTy();

          Triple TargetTriple(F.getParent()->getTargetTriple());
          Type *SizeTy = IRB.getInt64Ty();
          if (TargetTriple.isArch32Bit())
            SizeTy = IRB.getInt32Ty();

          if (IsBZero) {
            insertProfilePoint(IRB, CI, N+"-full", CI->getArgOperand(1));
            IRB.CreateCall2(IF.CPIDeleteRangeFn,
                  IRB.CreatePointerCast(CI->getArgOperand(0), Int8PtrTy),
                  IRB.CreateZExt(CI->getArgOperand(1), SizeTy));
          } else if (IsMemSet) {
            insertProfilePoint(IRB, CI, N+"-full", CI->getArgOperand(2));
            IRB.CreateCall2(IF.CPIDeleteRangeFn,
                  IRB.CreatePointerCast(CI->getArgOperand(0), Int8PtrTy),
                  IRB.CreateZExt(CI->getArgOperand(2), SizeTy));
          } else if (IsMemCpy) {
            insertProfilePoint(IRB, CI, N+"-full", CI->getArgOperand(2));
            IRB.CreateCall3(IF.CPICopyRangeFn,
                  IRB.CreatePointerCast(CI->getArgOperand(0), Int8PtrTy),
                  IRB.CreatePointerCast(CI->getArgOperand(1), Int8PtrTy),
                  IRB.CreateZExt(CI->getArgOperand(2), SizeTy));
          } else {
            assert(IsMemMove || IsBCopy);
            insertProfilePoint(IRB, CI, N+"-full", CI->getArgOperand(2));
            IRB.CreateCall3(IF.CPIMoveRangeFn,
                  IRB.CreatePointerCast(DstOp, Int8PtrTy),
                  IRB.CreatePointerCast(SrcOp, Int8PtrTy),
                  IRB.CreateZExt(CI->getArgOperand(2), SizeTy));
          }
        } while(0);

      } else {
        assert(RealTy);
        bool r1 = shouldProtectValue(DstOp, true, CPSOnly, NULL, RealTy);
        bool r2 = !r1 && RealTy2 &&
                  shouldProtectValue(DstOp, true, CPSOnly, NULL, RealTy2);

        if (r1 || r2) {
          ++NumProtectedMemcpyLikeOps;

          // DstOp is not char* and we need to protect it. Do type-based update.
          TargetFolder TF(DL);
          IRBuilder<true, TargetFolder> IRB(C, TF);
          IRB.SetInsertPoint(CI->getNextNode());
          IRB.SetCurrentDebugLocation(CI->getDebugLoc());

          Value *RealOp = IRB.CreateBitCast(DstOp,
                    r1 ? RealTy->getPointerTo() : RealTy2->getPointerTo());
          Value *Size = CI->getArgOperand(IsBZero ? 1 : 2);
          Value *PPt = insertProfilePoint(IRB, CI, N+"-ty-loop",
                                          IRB.getInt64(0));
          buildMetadataReloadLoop(IRB, RealOp, Size, PPt);
          It.getBasicBlockIterator() = IRB.GetInsertBlock();
          It.getInstructionIterator() = IRB.GetInsertPoint();
        }
      }
    // } else if (!CPSOnly && isCallocLikeFn(CI, TLI, true)) {
    } else if (CPSOnly && SoftwareMode && isCallocLikeFn(CI, TLI, true)) {      
      ++NumAllocFreeOps;

      TargetFolder TF(DL);
      IRBuilder<true, TargetFolder> IRB(C, TF);
      IRB.SetInsertPoint(CI->getNextNode());
      IRB.SetCurrentDebugLocation(CI->getDebugLoc());

      Type *RealTy = guessRealValueType(CI, &F);

      if (RealTy && shouldProtectValue(CI, true, CPSOnly, NULL, RealTy)) {
        ++NumProtectedAllocFreeOps;
        Value *RealOp = IRB.CreateBitCast(CI, RealTy->getPointerTo());
        Value *Size = IRB.CreateMul(CI->getArgOperand(0),
                                    CI->getArgOperand(1));
        Value *PPt = insertProfilePoint(IRB, CI,
                                        "calloc-ty-loop", IRB.getInt64(0));
        buildMetadataReloadLoop(IRB, RealOp, Size, PPt);
        It.getBasicBlockIterator() = IRB.GetInsertBlock();
        It.getInstructionIterator() = IRB.GetInsertPoint();

      } else if (!RealTy) {
        ++NumProtectedAllocFreeOps;
#ifdef CPI_PROFILE_STATS
        insertProfilePoint(IRB, CI, "calloc",
                           IRB.CreateMul(CI->getArgOperand(0),
                                         CI->getArgOperand(1)));
#endif

#warning Pass real size
        IRB.CreateCall(IF.CPIAllocFn,
              IRB.CreatePointerCast(CI, IRB.getInt8PtrTy()));
      }

    } else if (false && !CPSOnly && isMallocLikeFn(CI, TLI, true) &&
               (N.startswith("Z") || N.startswith("_Z"))) {
      ++NumAllocFreeOps;

      TargetFolder TF(DL);
      IRBuilder<true, TargetFolder> IRB(C, TF);
      IRB.SetInsertPoint(CI->getNextNode());
      IRB.SetCurrentDebugLocation(CI->getDebugLoc());

      // FIXME: unfortunately, we currently lack info
      Type *RealTy = guessRealValueType(CI, &F);

      if (RealTy && shouldProtectValue(CI, true, CPSOnly, NULL, RealTy)) {
        ++NumProtectedAllocFreeOps;
        Value *RealOp = IRB.CreateBitCast(CI, RealTy->getPointerTo());
        Value *Size = CI->getArgOperand(N == "posix_memalign" ? 2:0);
        Value *PPt = insertProfilePoint(IRB, CI,
                                        "calloc-ty-loop", IRB.getInt64(0));
        buildMetadataReloadLoop(IRB, RealOp, Size, PPt);
        It.getBasicBlockIterator() = IRB.GetInsertBlock();
        It.getInstructionIterator() = IRB.GetInsertPoint();

      } else if (!RealTy) {
        ++NumProtectedAllocFreeOps;
#ifdef CPI_PROFILE_STATS
        Value *Size = CI->getArgOperand(N == "posix_memalign" ? 2:0);
        insertProfilePoint(IRB, CI, "calloc", Size);
#endif

#warning Pass real size
        IRB.CreateCall(IF.CPIAllocFn,
              IRB.CreatePointerCast(CI, IRB.getInt8PtrTy()));
      }

    } else if (false && !CPSOnly && isFreeCall(CI, TLI)) {
      ++NumAllocFreeOps;

      Value *Op = CI->getArgOperand(0);
      TargetFolder TF(DL);
      IRBuilder<true, TargetFolder> IRB(C, TF);
      IRB.SetInsertPoint(CI);

      Type *RealTy = guessRealValueType(Op, &F);

      if (RealTy && shouldProtectValue(Op, true, CPSOnly, NULL, RealTy)) {
        ++NumProtectedAllocFreeOps;
        Value *RealOp = IRB.CreateBitCast(Op, RealTy->getPointerTo());
        Value *Size = IRB.CreateCall(IF.CPIMallocSizeFn, Op);
        Value *PPt = insertProfilePoint(IRB, CI,
                                        "free-ty-loop", IRB.getInt64(0));
        buildMetadataReloadLoop(IRB, RealOp, Size, PPt);

        It.getBasicBlockIterator() = CI->getParent();
        It.getInstructionIterator() = CI->getNextNode();

      } else if (!RealTy) {
        ++NumProtectedAllocFreeOps;
#ifdef CPI_PROFILE_STATS
        insertProfilePoint(IRB, CI, "free-full",
                           IRB.CreateCall(IF.CPIMallocSizeFn, Op));
#endif
        IRB.CreateCall(IF.CPIFreeFn, Op);
      }

    } else if (CPSOnly && SoftwareMode && isReallocLikeFn(CI, TLI, true)) {
#warning Should not do it on every realloc!
      ++NumAllocFreeOps;

      TargetFolder TF(DL);
      IRBuilder<true, TargetFolder> IRB(C, TF);

      Type *RealTy = guessRealValueType(CI, &F);
      if (!RealTy)
        RealTy = guessRealValueType(CI->getArgOperand(0), &F);

      if (RealTy && shouldProtectValue(CI, true, CPSOnly, NULL, RealTy)) {
        ++NumProtectedAllocFreeOps;
        IRB.SetInsertPoint(CI->getNextNode());
        IRB.SetCurrentDebugLocation(CI->getDebugLoc());

        Value *RealOp = IRB.CreateBitCast(CI, RealTy->getPointerTo());
        Value *Size = CI->getArgOperand(1);
        Value *PPt = insertProfilePoint(IRB, CI,
                                        "realloc-ty-loop", IRB.getInt64(0));
        buildMetadataReloadLoop(IRB, RealOp, Size, PPt);
        It.getBasicBlockIterator() = IRB.GetInsertBlock();
        It.getInstructionIterator() = IRB.GetInsertPoint();

      } else if (!RealTy) {
        ++NumProtectedAllocFreeOps;
        IRB.SetInsertPoint(CI);

        IRBuilder<> IRB(CI);
        Value *SizeOld =
            IRB.CreateCall(IF.CPIMallocSizeFn, CI->getArgOperand(0));

        IRB.SetInsertPoint(CI->getNextNode());
        IRB.SetCurrentDebugLocation(CI->getDebugLoc());

        insertProfilePoint(IRB, CI, "realloc", CI->getArgOperand(1));
        IRB.CreateCall4(IF.CPIReallocFn, CI, CI->getArgOperand(1),
                        CI->getArgOperand(0), SizeOld);
      }
    } else if (N == "sigaction") {
      Value *Ptr = CI->getArgOperand(2);
      if (Ptr->getType()->isPointerTy() &&
          cast<PointerType>(Ptr->getType())->getElementType()->isStructTy()) {
        Instruction *NextI = CI->getNextNode();
        BasicBlock *CondBB = CI->getParent();
        BasicBlock *NotNullBB = CondBB->splitBasicBlock(NextI);
        BasicBlock *NextBB = NotNullBB->splitBasicBlock(NextI);

        TargetFolder TF(DL);
        IRBuilder<true, TargetFolder> IRB(C, TF);

        IRB.SetInsertPoint(CondBB->getTerminator());
        IRB.SetCurrentDebugLocation(CI->getDebugLoc());
        IRB.CreateCondBr(
              IRB.CreateICmpNE(Ptr, Constant::getNullValue(Ptr->getType())),
              NotNullBB, NextBB);

        IRB.SetInsertPoint(NotNullBB, NotNullBB->begin());
        CondBB->back().eraseFromParent();

        Value *PPt = insertProfilePoint(IRB, CI, N + "-fixup");
        buildMetadataReload(IRB, Ptr, NULL, NULL, PPt);
        It.getBasicBlockIterator() = NextBB;
        It.getInstructionIterator() = NextI;
      }
    } else if (!CPSOnly) {
      // Temporary hacks to compensate the fact that we don't instrument libc++
      if (N == "_ZNSt14basic_ifstreamIcSt11char_traitsIcEEC1EPKcSt13_Ios_Openmode" ||
          N == "_ZNSt14basic_ofstreamIcSt11char_traitsIcEEC1Ev") {
        TargetFolder TF(DL);
        IRBuilder<true, TargetFolder> IRB(C, TF);
        IRB.SetInsertPoint(CI->getNextNode());
        IRB.SetCurrentDebugLocation(CI->getDebugLoc());
        Value *PPt = insertProfilePoint(IRB, CI, "cpp-fixup");
        buildMetadataReload(IRB, CI->getArgOperand(0), NULL, NULL, PPt);
        It.getBasicBlockIterator() = IRB.GetInsertBlock();
        It.getInstructionIterator() = IRB.GetInsertPoint();
      }
    }
  }

  TargetFolder TF(DL);
  IRBuilder<true, TargetFolder> IRB(C, TF);
  IRB.SetInsertPoint(F.getEntryBlock().getFirstInsertionPt());

  Value *PPt = NULL;

  // Now, instrument all by-val arguments
  for (Function::arg_iterator It = F.arg_begin(),
                              Ie = F.arg_end(); It != Ie; ++It) {
    Argument *A = It;
    if (!A->hasByValAttr())
      continue;

    ++NumStores;
    assert(A->getType()->isPointerTy());
    if (!shouldProtectType(cast<PointerType>(A->getType())->getElementType(),
                           true, CPSOnly))
      continue;

    ++NumProtectedStores;
    if (!PPt)
      PPt = insertProfilePoint(IRB, F.getEntryBlock().getFirstNonPHI(),
                               "byval-args", IRB.getInt64(0));

    buildMetadataReload(IRB, A, NULL, NULL, PPt);
  }

  // Finally, replace all loads from ReplMap
  if (!CPIDebugMode) {
    for (unsigned i = 0, e = ReplMap.size(); i != e; ++i) {
      Instruction *From = ReplMap[i].first, *To = ReplMap[i].second;
      To->takeName(From);
      From->replaceAllUsesWith(To);
    }
  }

  return true;
}

Function *CPI::createGlobalsReload(Module &M, StringRef N,
                                                        bool OnlyDeclarations) {
  LLVMContext &C = M.getContext();
  Function *F = Function::Create(
      FunctionType::get(Type::getVoidTy(C), false),
      GlobalValue::InternalLinkage, N, &M);

  TargetFolder TF(DL);
  IRBuilder<true, TargetFolder> IRB(C, TF);

  BasicBlock *Entry = BasicBlock::Create(C, "", F);
  IRB.SetInsertPoint(Entry);

  Instruction *CI = IRB.CreateCall(IF.CPIInitFn);

  /*
  IRB.CreateCall(IF.CPIDumpFn, IRB.CreateIntToPtr(IRB.getInt64(0),
                                           IRB.getInt8PtrTy()->getPointerTo()));
                                           */

  Value *PPt = insertProfilePoint(IRB, CI, "globals", IRB.getInt64(0));

  IRB.CreateRetVoid();
  IRB.SetInsertPoint(IRB.GetInsertBlock(),
                     IRB.GetInsertBlock()->getTerminator());

  for (Module::global_iterator It = M.global_begin(),
                               Ie = M.global_end(); It != Ie; ++It) {
    GlobalVariable *GV = &*It;

    if (GV->getName().startswith("llvm.") ||
        GV->getName().startswith("__llvm__"))
      continue;

    if (OnlyDeclarations && !GV->isDeclaration())
      continue;

    ++NumInitStores;

    // FIXME: in fact, we might not want to protect i8 pointers when
    // loading globals, as those are likely to have correct type anyway.
    if (!shouldProtectType(GV->getType()->getElementType(), true, false)) {
                           //!HasCPIFullFunctions)) {
      //outs() << "NOT Protect: " << GV->getName() << "\n";
      continue;
    }

    ++NumProtectedInitStores;

    //outs() << "Protect: " << GV->getName() << "\n";
    buildMetadataReload(IRB, GV, NULL, NULL, PPt);
  }

  return F;
}

bool CPI::doCPIFinalization(Module &M) {
  if (SoftwareMode) {
    Function *F1 = createGlobalsReload(M, "__llvm__cpi.module_init", false);
    appendToGlobalCtors(M, F1, 0);

    // FIXME: this is a hack that only works with lto
    if (HasCPIFullFunctions) {
      Function *Main = M.getFunction("main");
      if (Main != NULL && !Main->isDeclaration()) {
        Function *F2 = createGlobalsReload(M,
                                           "__llvm__cpi.module_pre_main", true);
        F2->addFnAttr(Attribute::NoInline);
        CallInst::Create(F2, Twine(),
                         cast<Instruction>(Main->getEntryBlock().getFirstNonPHI()));
      }
    }
  } else { // !SoftwareMode
    bool CPI = false;
    for (Module::iterator It = M.begin(), Ie = M.end(); It != Ie; ++It) {
      Function &F = *It;
      if (F.hasFnAttribute("cpi")) {
        CPI = true;
        break;
      }
    }

    if (CPI) {
      Function *F1 = createGlobalsReload(M, "__llvm__cpi.module_init", false);
      appendToGlobalCtors(M, F1, 0);

      // FIXME: this is a hack that only works with lto
      if (HasCPIFullFunctions) {
        Function *Main = M.getFunction("main");
        if (Main != NULL && !Main->isDeclaration()) {
          Function *F2 = createGlobalsReload(M,
                                             "__llvm__cpi.module_pre_main", true);
          F2->addFnAttr(Attribute::NoInline);
          CallInst::Create(F2, Twine(),
                           cast<Instruction>(Main->getEntryBlock().getFirstNonPHI()));
        }
      }
    }
  } // end of !SoftwareMode

  doStatsFinalization(M);
  return true;
}
