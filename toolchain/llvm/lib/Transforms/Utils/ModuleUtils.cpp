//===-- ModuleUtils.cpp - Functions to manipulate Modules -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This family of functions perform manipulations on Modules.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

using namespace llvm;

void llvm::appendToGlobalArray(Module &M, const char *Array,
                         Constant *NewEntry) {
  // Get the current set of entries and add the new entry to the list.
  SmallVector<Constant *, 16> CurrentEntries;
  if (GlobalVariable * GV = M.getNamedGlobal(Array)) {
    if (Constant *Init = GV->getInitializer()) {
      unsigned n = Init->getNumOperands();
      CurrentEntries.reserve(n + 1);
      for (unsigned i = 0; i != n; ++i) {
        assert(Init->getOperand(i)->getType() == NewEntry->getType());
        CurrentEntries.push_back(cast<Constant>(Init->getOperand(i)));
      }
    }
    GV->eraseFromParent();
  }

  CurrentEntries.push_back(NewEntry);

  // Create a new initializer.
  ArrayType *AT = ArrayType::get(NewEntry->getType(), CurrentEntries.size());
  Constant *NewInit = ConstantArray::get(AT, CurrentEntries);

  // Create the new global variable and replace all uses of
  // the old global variable with the new one.
  (void)new GlobalVariable(M, NewInit->getType(), false,
                           GlobalValue::AppendingLinkage, NewInit, Array);
}

static Constant *getCtorDtorEntry(Function *F, int Priority) {
  IRBuilder<> IRB(F->getContext());
  FunctionType *FnTy = FunctionType::get(IRB.getVoidTy(), false);
  StructType *Ty = StructType::get(
      IRB.getInt32Ty(), PointerType::getUnqual(FnTy), NULL);

  return ConstantStruct::get(Ty, IRB.getInt32(Priority), F, NULL);
}

void llvm::appendToGlobalCtors(Module &M, Function *F, int Priority) {
  appendToGlobalArray(M, "llvm.global_ctors", getCtorDtorEntry(F, Priority));
}

void llvm::appendToGlobalDtors(Module &M, Function *F, int Priority) {
  appendToGlobalArray(M, "llvm.global_dtors", getCtorDtorEntry(F, Priority));
}
