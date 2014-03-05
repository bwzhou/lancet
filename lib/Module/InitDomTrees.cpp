#include "Passes.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Pass.h"
#include <iostream>
using namespace llvm;

namespace klee {
  char InitDomTrees::ID;

  void InitDomTrees::populate(PostDominatorTree *PDT, Function *F) {
    for (Function::iterator i = F->begin(); i != F->end(); ++i) {
      for (Function::iterator j = F->begin(); j != F->end(); ++j) {
        /*
         * std::cerr << __FILE__ << ":" << __LINE__
         *           << " pdom " << PDT << " check if "
         *           << j << " dominates " << i << " = " << PDT->dominates(j, i)
         *           << std::endl;
         */
        (*dom)[make_pair(i, j)] = PDT->properlyDominates(i, j);
      }
    }
  }

  bool InitDomTrees::runOnModule(Module &M) {
    std::cerr << __FILE__ << ":" << __LINE__ << " InitDomTrees" << std::endl;
    for (Module::iterator it = M.begin(), ie = M.end(); it != ie; ++it) {
      // http://lists.cs.uiuc.edu/pipermail/llvmdev/2011-March/038491.html
      if (it->isDeclaration() == false) {
        PostDominatorTree *PDT = &getAnalysis<PostDominatorTree>(*it);
        populate(PDT, it);
      }
    }
    return false;
  }
  void InitDomTrees::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<PostDominatorTree>();
  }
}
