#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/GraphWriter.h"

using namespace llvm;

void writeMemorySSADOT(const Function &F, MemorySSA &MSSA) {
  std::string Filename = "mssa-" + F.getName().str() + ".dot";
  std::error_code EC;
  raw_fd_ostream File(Filename, EC);

  File << "digraph \"MSSA-" << F.getName() << "\" {\n";
  File << " node [shape=box];\n";

  for (auto &BB : F) {
    if (auto *Access = MSSA.getMemoryAccess(&BB)) {
      if (auto *Phi = dyn_cast<MemoryPhi>(Access)) {
        File << " \"" << Phi << "\" [label=\"MemoryPhi\"];\n";
        for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
          auto *Inc = Phi->getIncomingValue(i);
          File << " \"" << Inc << "\" -> \"" << Phi << "\";\n";
        }
      }
    }
  }

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *MA = MSSA.getMemoryAccess(&I)) {
        File << " \"" << MA << "\" [label=\"";
        MA->print(File);
        File << "\"];\n";

        if (auto *Def = dyn_cast<MemoryDef>(MA)) {
          File << " \"" << Def->getDefiningAccess() << "\" -> \""
          << Def << "\";\n";
        }

        if (auto *Use = dyn_cast<MemoryUse>(MA)) {
          File << " \"" << Use->getDefiningAccess()
          << "\" -> \"" << Use << "\";\n";
        }
      }
    }
  }

  File << "}\n";

  errs() << "Wrote MemorySSA graph to " << Filename << "\n";
}

struct MemorySSADemoPass : PassInfoMixin<MemorySSADemoPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    auto &MSSAResult = AM.getResult<MemorySSAAnalysis>(F);
    auto &MSSA = MSSAResult.getMSSA();

    writeMemorySSADOT(F, MSSA);

    errs() << "Analyzing function: " << F.getName() << "\n";

    // Iterate over basic blocks to show all MemoryAccesses
    for (auto &BB : F) {
      errs() << "BasicBlock: " << BB.getName() << "\n";

      // MemoryPhi nodes are found at block entries
      if (auto *Phi = MSSA.getMemoryAccess(&BB)) {
        if (auto *MPhi = dyn_cast<MemoryPhi>(Phi)) {
          errs() << "  MemoryPhi for block " << BB.getName() << ":\n";
          for (unsigned i = 0; i < MPhi->getNumIncomingValues(); ++i) {
            auto *IncomingAcc = MPhi->getIncomingValue(i);
            auto *Pred = MPhi->getIncomingBlock(i);
            errs() << "    from " << Pred->getName() << ": ";
            IncomingAcc->print(errs());
            errs() << "\n";
          }
        }
      }

      // Iterate over instructions for MemoryDef/Use
      for (auto &I : BB) {
        if (auto *MA = MSSA.getMemoryAccess(&I)) {
          errs() << "  ";
          MA->print(errs());
          errs() << "\n";
        }
      }
    }

    return PreservedAnalyses::all();
  }
};

extern"C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MemorySSADemoPass", "v0.9",
          [](PassBuilder &PB) {
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([] { return MemorySSAAnalysis(); });
                });

            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "memssa-demo") {
                    FPM.addPass(MemorySSADemoPass());
                    return true;
                  }
                  return false;
                });
          }};
}

