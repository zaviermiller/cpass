#include <llvm/Support/Casting.h>

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Format.h"

#define SRC_IDX 0
#define DST_IDX 1

using namespace llvm;
using namespace std;

typedef map<Value *, Value *> ACPTable;

class BasicBlockInfo {
 public:
  BitVector COPY;
  BitVector KILL;
  BitVector CPIn;
  BitVector CPOut;
  ACPTable ACP;

  BasicBlockInfo(unsigned int max_copies) {
    COPY.resize(max_copies);
    KILL.resize(max_copies);
    CPIn.resize(max_copies);
    CPOut.resize(max_copies);
  }
};

namespace {
class CopyPropagation : public FunctionPass {
 private:
  void localCopyPropagation(Function &F);
  void globalCopyPropagation(Function &F);
  void propagateCopies(BasicBlock &bb, ACPTable &acp);

 public:
  static char ID;
  static cl::opt<bool> verbose;
  CopyPropagation() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    localCopyPropagation(F);
    globalCopyPropagation(F);
    return true;
  }
};  // end CopyPropagation

class DataFlowAnalysis {
 private:
  /* LLVM does not store the position of instructions in the Instruction
   * class, so we create maps of the store instructions to make them
   * easier to use and reference in the BitVector objects
   */
  std::vector<Value *> copies;
  std::map<Value *, int> copy_idx;
  std::map<int, Value *> idx_copy;
  std::map<BasicBlock *, BasicBlockInfo *> bb_info;
  unsigned int nr_copies;

  void addCopy(Value *v);
  void initCopyIdxs(Function &F);
  void initCOPYAndKILLSets(Function &F);
  void initCPInAndCPOutSets(Function &F);
  void initACPs();

 public:
  DataFlowAnalysis(Function &F);
  ACPTable &getACP(BasicBlock &bb);
  void printCopyIdxs();
  void printDFA();
};  // end DataFlowAnalysis
}  // end anonymous namespace

char CopyPropagation::ID = 0;
static RegisterPass<CopyPropagation> X("copy_prop", "copy_prop",
                                       false /* Only looks at CFG */,
                                       false /* Analysis Pass */);

cl::opt<bool> CopyPropagation::verbose("verbose",
                                       cl::desc("turn on verbose printing"),
                                       cl::init(false));

ACPTable::const_iterator findValueInACP(const ACPTable &acp,
                                        Value *search_value) {
  for (auto it = acp.begin(); it != acp.end(); ++it) {
    if (it->second == search_value) return it;
  }
  return acp.end();
}

/*
 * propagateCopies performs copy propagation over the block bb using the
 * available copy instructions in the table acp. It will also remove load
 * instructions if they are no longer useful.
 *
 * Useful tips:
 *
 * Use C++ features to iterate over the instructions in a block, e.g.:
 *   Instruction *iptr;
 *   for (Instruction &ins : bb) {
 *     iptr = &ins;
 *     ...
 *   }
 *
 * You can use isa to determine the type of an instruction, e.g.:
 *   if (isa<StoreInst>(iptr)) {
 *     // iptr points to an Instruction that is a StoreInst
 *   }
 *
 * Other useful LLVM routines:
 *   int  Instruction::getOperand(int)
 *   void Instruction::setOperand(int)
 *   int  Instruction::getNumOperands()
 *   void Instruction::eraseFromParent()
 */
void CopyPropagation::propagateCopies(BasicBlock &bb, ACPTable &acp) {
  vector<Instruction *> to_remove;
  Instruction *iptr;
  Value *dest, *src, *op;
  int i;

  for (Instruction &ins : bb) {
    iptr = &ins;

    // found a store instruction
    if (isa<StoreInst>(iptr)) {
      dest = ins.getOperand(1);
      src = ins.getOperand(0);

      if (acp.find(dest) != acp.end()) {
        // find all values in acp equal to dest and remove those
        for (auto it = acp.begin(); it != acp.end();) {
          if (it->second == dest) {
            it = acp.erase(it);
          } else {
            it++;
          }
        }
        // erase dest
        acp.erase(dest);
      }

      if (acp.find(src) != acp.end()) {
        ins.setOperand(0, acp[src]);
        acp[dest] = acp[src];
      } else {
        acp[dest] = src;
      }

    } else if (isa<LoadInst>(iptr)) {
      // found a load inst, associate the destination of
      // the load with whats being stored and remove the instruction
      dest = (Value *)iptr;
      src = ins.getOperand(0);
      // if the load instruction is pulling from something in the acp
      if (acp.find(src) != acp.end()) {
        acp[dest] = acp[src];
        // add to list of instructions to remove
        to_remove.push_back(iptr);
      }
    } else {
      // replace uses in acp when encountering any other instruction
      for (i = 0; i < ins.getNumOperands(); i++) {
        Value *op = ins.getOperand(i);
        if (acp.find(op) != acp.end()) {
          ins.setOperand(i, acp[op]);
        }
      }
    }
  }

  // remove all the redundant loads
  for (Instruction *ins : to_remove) {
    ins->eraseFromParent();
  }
}

/*
 * localCopyPropagation performs local copy propagation (LCP) over the basic
 * blocks in the function F. The algorithm for LCP described on pp. 357-358 in
 * the provided text (Muchnick).
 *
 * Useful tips:
 *
 * Use C++ features to iterate over the blocks in F, e.g.:
 *   for (BasicBlock &bb : F) {
 *     ...
 *   }
 *
 * This routine should call propagateCopies
 */
void CopyPropagation::localCopyPropagation(Function &F) {
  ACPTable acp;

  for (BasicBlock &bb : F) {
    propagateCopies(bb, acp);
    // clear out acp between each run
    acp.clear();
  }

  // debug
  if (verbose) {
    errs() << "post local"
           << "\n"
           << (*(&F)) << "\n";
  }
}

/*
 * globalCopyPropagation performs global copy propagation (LCP) over the basic
 * blocks in the function F. The algorithm for GCP described on pp. 358-360 in
 * the provided text (Muchnick).
 *
 * Useful tips:
 *
 * This routine will use the DataFlowAnalysis to construct COPY, KILL, CPIn,
 * and CPOut sets and an ACP table for each block.
 *
 * Use C++ features to iterate over the blocks in F, e.g.:
 *   for (BasicBlock &bb : F) {
 *     ...
 *   }
 *
 * This routine should also call propagateCopies
 */
void CopyPropagation::globalCopyPropagation(Function &F) {
  DataFlowAnalysis *dfa;
  ACPTable acp;
  dfa = new DataFlowAnalysis(F);

  // implement global copy propagation here

  for (BasicBlock &bb : F) {
    acp = dfa->getACP(bb);
    propagateCopies(bb, acp);
  }

  if (verbose) {
    errs() << "post global"
           << "\n"
           << (*(&F)) << "\n";
  }
}

/*
 * addCopy is a helper routine for initCopyIdxs. It updates state information
 * to record the index of a single copy instruction
 */
void DataFlowAnalysis::addCopy(Value *v) {
  // add copy if it doesnt already exist
  if (copy_idx.find(v) == copy_idx.end()) {
    int idx = nr_copies++;
    copy_idx[v] = idx;
    idx_copy[idx] = v;
    copies.push_back(v);
  }
}

/*
 * initCopyIdxs creates a table that records unique identifiers for each copy
 * (i.e., argument and store) instructions in LLVM.
 *
 * LLVM does not store the position of instructions in the Instruction class,
 * so this routine is used to record unique identifiers for each copy
 * instruction in the Function F. This step makes it easier to identify copy
 * instructions in the COPY, KILL, CPIn, and CPOut sets.
 *
 * Useful tips:
 *
 * You should record function arguments and store instructions as copy
 * instructions.
 *
 * Some useful LLVM routines in this routine are:
 *   Function::arg_iterator Function::arg_begin()
 *   Function::arg_iterator Function::arg_end()
 *   bool llvm::isa<T>(Instruction *)
 */
void DataFlowAnalysis::initCopyIdxs(Function &F) {
  // add copy for all function args
  for (auto ai = F.arg_begin(); ai != F.arg_end(); ai++) {
    addCopy(&(*ai));
  }

  // iterate over all instructions and add copy for each store inst
  for (BasicBlock &bb : F) {
    for (Instruction &i : bb) {
      if (isa<StoreInst>(&i)) {
        addCopy(&i);
      }
    }
  }
}

/*
 * initCOPYAndKILLSets initializes the COPY and KILL sets for each basic block
 * in the function F.
 *
 * Useful tips:
 *
 * This routine should visit the blocks in reverse post order. You can use an
 * LLVM iterator to complete this traversal, e.g.:
 *
 *   BasicBlock *bb;
 *   ReversePostOrderTraversal<Function*> RPOT(&F);
 *   for ( auto BB = RPOT.begin(); BB != RPOT.end(); BB++ ) {
 *       bb = *BB;
 *       ...
 *   }
 *
 * This routine should create BasicBlockInfo objects for each basic block and
 * record the BasicBlockInfo for each block in the bb_info map.
 *
 * Some useful LLVM routines in this routine are:
 *   bool llvm::isa<T>(Instruction *)
 *   int  Instruction::getOperand(int)
 */
void DataFlowAnalysis::initCOPYAndKILLSets(Function &F) {
  BasicBlock *bb;
  BasicBlockInfo *bbi;
  Value *dest, *op, *other_dest;
  Instruction *op_ins;
  ReversePostOrderTraversal<Function *> RPOT(&F);

  for (auto BB = RPOT.begin(); BB != RPOT.end(); BB++) {
    bb = *BB;
    bbi = new BasicBlockInfo(nr_copies);  // change?
    this->bb_info[bb] = bbi;

    for (Instruction &ins : *bb) {
      if (isa<StoreInst>(ins)) {
        dest = ins.getOperand(1);
        bbi->COPY.set(copy_idx[&ins]);

        // to generate KILL we need to get instructions that modify the dest of
        // a COPY outside of this block

        // iterate through idx_copy
        for (auto it = idx_copy.begin(); it != idx_copy.end(); it++) {
          op = it->second;  // know we only put instructions here
          if (isa<Instruction>(op)) {
            op_ins = (Instruction *)op;
            other_dest = op_ins->getOperand(1);
            // dont do anything if the other instruction is in the same block
            if (op_ins->getParent() == bb) {
              continue;
            }
            // add to kill set
            if (other_dest == dest) {
              bbi->KILL.set(it->first);
            }
          } else {
            // add function arg to kill set
            if (op == dest) {
              bbi->KILL.set(it->first);
            }
          }

        }

        // dont set the kill set for this instruction
        bbi->KILL.reset(copy_idx[&ins]);
      }
    }
  }
}

/*
 * initCPInAndCPOutSets initializes the CPIn and CPOut sets for each basic
 * block in the function F.
 *
 * Useful tips:
 *
 * Similar to initCOPYAndKillSets, you will need to traverse the blocks in
 * reverse post order.
 *
 * You can iterate the predecessors and successors of a block bb using
 * LLVM-defined iterators "predecessors" and "successors", e.g.:
 *
 *   for ( BasicBlock* pred : predecessors( bb ) ) {
 *       // pred points to a predecessor of bb
 *       ...
 *   }
 *
 * You will need to define a special case for the entry block (and some way to
 * identify the entry block).
 *
 *
 * Use set operations on the appropriate BitVector to create CPIn and CPOut.
 */
void DataFlowAnalysis::initCPInAndCPOutSets(Function &F) {
  BasicBlock *bb;
  ReversePostOrderTraversal<Function *> RPOT(&F);
  BasicBlockInfo *bbi, *pbbi;

  bool changed = false;
  bool initial = true;

loop:
  do {
    changed = false;
    for (auto BB = RPOT.begin(); BB != RPOT.end(); BB++) {
      bb = *BB;
      bbi = bb_info[bb];
      BitVector cpout_copy(bbi->CPOut);
      BitVector cpin_copy(bbi->CPIn);

      for (BasicBlock *pred : predecessors(bb)) {
        pbbi = bb_info[pred];
        for (int i = 0; i < pbbi->CPOut.size(); i++) {
          // during initial DFA create union of possible CPIn sets
          if (initial) {
            bbi->CPIn[i] = bbi->CPIn[i] | pbbi->CPOut[i];
          } else {
            // after initial pass, only set CPIn to CPOut from all preds
            bbi->CPIn[i] = bbi->CPIn[i] & pbbi->CPOut[i];
          }
        }
      }

      // detect if there was a change (maybe could change)
      if (bbi->CPIn != cpin_copy) {
        changed = true;
      }

      // compute cpout using book alg
      for (int i = 0; i < bbi->CPOut.size(); i++) {
        bbi->CPOut[i] = bbi->COPY[i] | (bbi->CPIn[i] & (~bbi->KILL[i]));
      }

      // detect change
      if (bbi->CPOut != cpout_copy) {
        changed = true;
      }
    }
  } while (changed);

  // after initial DFA, go back and compute CPIn and CPOut
  if (initial) {
    initial = false;
    goto loop;
  }


  // do {
  //   changed = false;
  //   for (auto BB = RPOT.begin(); BB != RPOT.end(); BB++) {
  //     bb = *BB;
  //     BasicBlockInfo *bbi = bb_info[bb];
  //     BitVector cpout_copy(bbi->CPOut);
  //     BitVector cpin_copy(bbi->CPIn);
  //     for (BasicBlock *pred : predecessors(bb)) {
  //       BasicBlockInfo *pbbi = bb_info[pred];
  //       for (int i = 0; i < pbbi->CPOut.size(); i++) {
  //         bbi->CPIn[i] = bbi->CPIn[i] & pbbi->CPOut[i];
  //       }
  //     }

  //     if (bbi->CPIn != cpin_copy) {
  //       changed = true;
  //     }

  //     // compute cpout
  //     for (int i = 0; i < bbi->CPOut.size(); i++) {
  //       bbi->CPOut[i] = bbi->COPY[i] | (bbi->CPIn[i] & (~bbi->KILL[i]));
  //     }

  //     if (bbi->CPOut != cpout_copy) {
  //       changed = true;
  //     }
  //   }
  // } while(changed);
}




/*
 * initACPs creates an ACP table for each basic block, which will be used to
 * conduct global copy propagation.
 *
 * Useful tips:
 *
 * You will need to use CPIn to determine if a copy should be in the ACP for
 * this block.
 */
void DataFlowAnalysis::initACPs() {
  for (auto it = bb_info.begin(); it != bb_info.end(); it++) {
    BasicBlock *bb = it->first;
    BasicBlockInfo *bbi = it->second;

    for (int i = 0; i < nr_copies; i++) {
      if (bbi->CPIn[i]) {
        Instruction *ins = (Instruction *)idx_copy[i];
        Value *src = ins->getOperand(0);
        Value *dest = ins->getOperand(1);

        bbi->ACP[dest] = src;
      }
    }
  }
}

ACPTable &DataFlowAnalysis::getACP(BasicBlock &bb) {
  return bb_info[(&bb)]->ACP;
}

void DataFlowAnalysis::printCopyIdxs() {
  errs() << "copy_idx:"
         << "\n";
  for (auto it = copy_idx.begin(); it != copy_idx.end(); ++it) {
    errs() << "  " << format("%-3d", it->second) << " --> " << *(it->first)
           << "\n";
  }
  errs() << "\n";
}

void DataFlowAnalysis::printDFA() {
  unsigned int i;

  // used for formatting
  std::string str;
  llvm::raw_string_ostream rso(str);

  for (auto it = bb_info.begin(); it != bb_info.end(); ++it) {
    BasicBlockInfo *bbi = bb_info[it->first];

    errs() << "BB ";
    it->first->printAsOperand(errs(), false);
    errs() << "\n";

    errs() << "  CPIn  ";
    for (i = 0; i < bbi->CPIn.size(); i++) {
      errs() << bbi->CPIn[i] << ' ';
    }
    errs() << "\n";

    errs() << "  CPOut ";
    for (i = 0; i < bbi->CPOut.size(); i++) {
      errs() << bbi->CPOut[i] << ' ';
    }
    errs() << "\n";

    errs() << "  COPY  ";
    for (i = 0; i < bbi->COPY.size(); i++) {
      errs() << bbi->COPY[i] << ' ';
    }
    errs() << "\n";

    errs() << "  KILL  ";
    for (i = 0; i < bbi->KILL.size(); i++) {
      errs() << bbi->KILL[i] << ' ';
    }
    errs() << "\n";

    errs() << "  ACP:"
           << "\n";
    for (auto it = bbi->ACP.begin(); it != bbi->ACP.end(); ++it) {
      rso << *(it->first);
      errs() << "  " << format("%-30s", rso.str().c_str())
             << "==  " << *(it->second) << "\n";
      str.clear();
    }
    errs() << "\n"
           << "\n";
  }
}

/*
 * DataFlowAnalysis constructs the data flow analysis for the function F.
 *
 * You will not need to modify this routine.
 */
DataFlowAnalysis::DataFlowAnalysis(Function &F) {
  initCopyIdxs(F);
  initCOPYAndKILLSets(F);
  initCPInAndCPOutSets(F);
  initACPs();

  if (CopyPropagation::verbose) {
    errs() << "post DFA"
           << "\n";
    printCopyIdxs();
    printDFA();
  }
}
