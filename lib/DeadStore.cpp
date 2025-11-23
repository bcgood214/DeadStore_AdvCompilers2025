#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
    struct LocalDSEPass : PassInfoMixin<LocalDSEPass> {

        
    bool isStoreDead(MemoryDef *MD, AliasAnalysis &AA, MemoryLocation StoredLoc) {
        SmallVector<MemoryAccess *, 8> Worklist;
        SmallPtrSet<MemoryAccess *, 16> Visited;

        Worklist.push_back(MD);
        Visited.insert(MD);

        while (!Worklist.empty()) {
            MemoryAccess *MA = Worklist.pop_back_val();

            for (User *U : MA->users()) {
                auto *MAUser = dyn_cast<MemoryAccess>(U);
                if (!MAUser)
                    continue;

                // Any MemoryUse means this store is live
                if (isa<MemoryUse>(MAUser))
                    return false;

                // -----------------------
                // Handle MemoryPhi
                // -----------------------
                if (auto *MP = dyn_cast<MemoryPhi>(MAUser)) {

                    for (unsigned i = 0; i < MP->getNumIncomingValues(); i++) {
                        MemoryAccess *Incoming = MP->getIncomingValue(i);

                        // If an incoming edge uses MD -> store is live
                        if (isa<MemoryUse>(Incoming))
                            return false;

                        if (auto *IncomingMD = dyn_cast<MemoryDef>(Incoming)) {
                            Instruction *NextInst = IncomingMD->getMemoryInst();

                            if (auto *NextStore = dyn_cast<StoreInst>(NextInst)) {
                                MemoryLocation NextLoc = MemoryLocation::get(NextStore);

                                if (AA.alias(StoredLoc, NextLoc) == AliasResult::MustAlias) {
                                    // this edge kills our store
                                    continue;
                                }

                                // non-killing write -> store is live
                                return false;
                            }

                            // non-store MemoryDef: conservatively treat as live
                            return false;
                        }
                    }

                    // continue walking past the phi
                    if (Visited.insert(MP).second)
                        Worklist.push_back(MP);

                    continue;
                }

                // -----------------------
                // MemoryDef that is not a phi
                // -----------------------
                if (auto *NextMD = dyn_cast<MemoryDef>(MAUser)) {
                    Instruction *NextInst = NextMD->getMemoryInst();

                    if (auto *NextStore = dyn_cast<StoreInst>(NextInst)) {
                        MemoryLocation NextLoc = MemoryLocation::get(NextStore);

                        if (AA.alias(StoredLoc, NextLoc) == AliasResult::MustAlias) {
                            // This store overwrites the previous one
                            continue;
                        }

                        // store depends on old value
                        return false;
                    }

                    // Non-store MemoryDef → conservatively live
                    return false;
                }

                // continue walking the SSA graph
                if (Visited.insert(MAUser).second)
                    Worklist.push_back(MAUser);
            }
        }

        return true; // no uses, and all future edges kill MD
    }


    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        if (F.isDeclaration())
            return PreservedAnalyses::all();
        
        auto &MSSAResult = AM.getResult<MemorySSAAnalysis>(F);
        auto &MSSA = MSSAResult.getMSSA();

        auto &AA = AM.getResult<AAManager>(F);

        SmallVector<Instruction *, 16> ToErase;

        errs() << "Running LocalDSEPass on function: " << F.getName() << "\n";

        for (auto &BB : F) {
            for (auto &I : BB) {
                auto *SI = dyn_cast<StoreInst>(&I);
                if (!SI)
                    continue;
                
                if (SI->isVolatile())
                    continue;
                
                auto *MA = MSSA.getMemoryAccess(SI);
                if (!MA)
                    continue;
                
                auto *MD = dyn_cast<MemoryDef>(MA);
                if (!MD)
                    continue;
                
                MemoryLocation Loc = MemoryLocation::get(SI);
                
                if (isStoreDead(MD, AA, Loc)) {
                    errs() << " Dead store found, removing: ";
                    SI->print(errs());
                    errs() << "\n";
                    ToErase.push_back(SI);
                }
            }
        }

        MemorySSAUpdater MSSAUpdater(&MSSA);

        for (Instruction *Inst : ToErase) {
            if (auto *MD = dyn_cast_or_null<MemoryDef>(MSSA.getMemoryAccess(Inst))) {
                MSSAUpdater.removeMemoryAccess(MD);
            }
            Inst->eraseFromParent();
        }

        if (ToErase.empty()) {
            return PreservedAnalyses::all();
        }

        return PreservedAnalyses::none();
    }
};

}

// ChatGPT output was copied and pasted for the plugin registration and
// imports/namespace, otherwise, it was used as a learning tool.
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LocalDSEPass", "v0.1",
          [](PassBuilder &PB) {
            // Make sure MemorySSAAnalysis is available.
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([] { return MemorySSAAnalysis(); });
                  FAM.registerPass([] { return AAManager(); });
                });

            // Allow -passes="local-dse" on opt
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "local-dse") {
                    FPM.addPass(LocalDSEPass());
                    return true;
                  }
                  return false;
                });
          }};
}
