#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/CommandLine.h"

#include <llvm/Support/Casting.h>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <queue>

#define SRC_IDX 0
#define DST_IDX 1

using namespace llvm;
using namespace std;

typedef map<Value*, Value*> ACPTable;

class BasicBlockInfo {
  public:
    BitVector COPY;
    BitVector KILL;
    BitVector CPIn;
    BitVector CPOut;
    ACPTable  ACP;
    
    BasicBlockInfo(unsigned int max_copies)
    {
        COPY.resize(max_copies);
        KILL.resize(max_copies);
        CPIn.resize(max_copies);
        CPOut.resize(max_copies);
    }
};

namespace {
class CopyPropagation : public FunctionPass
{
  private:
    void localCopyPropagation( Function &F );
    void globalCopyPropagation( Function &F );
    void propagateCopies(BasicBlock &bb, ACPTable &acp);

  public:
    static char ID;
    static cl::opt<bool> verbose;
    CopyPropagation() : FunctionPass( ID ) {}

    bool runOnFunction( Function &F ) override
    {
        localCopyPropagation( F );
        globalCopyPropagation( F );
        return true;
    }
}; // end CopyPropagation

class DataFlowAnalysis
{
    private:
        /* LLVM does not store the position of instructions in the Instruction
         * class, so we create maps of the store instructions to make them
         * easier to use and reference in the BitVector objects
         */
        std::vector<Value*> copies;
        std::map<Value*, int> copy_idx;
        std::map<int, Value*> idx_copy;
        std::map<BasicBlock*, BasicBlockInfo*> bb_info;
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
}; // end DataFlowAnalysis
}  // end anonymous namespace

char CopyPropagation::ID = 0;
static RegisterPass<CopyPropagation> X("copy_prop", "copy_prop",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

cl::opt<bool> CopyPropagation::verbose( "verbose", cl::desc( "turn on verbose printing" ), cl::init( false ) );


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
void CopyPropagation::propagateCopies(BasicBlock &bb, ACPTable &acp)
{
  vector<Instruction *>to_remove;
  Instruction *iptr;
  int i;

  for (Instruction &ins : bb) {
    iptr = &ins;


    if (isa<StoreInst>(iptr)) {
      Value *dest, *src;
      dest = ins.getOperand(1);
      src = ins.getOperand(0);
      if (acp.find(src) != acp.end()) {
        acp[dest] = acp[src];
        // replace source with acp[src]
        ins.setOperand(0, acp[src]);
      } else {
        acp[dest] = src;
      }
    } else if (isa<LoadInst>(iptr)) {
      Value *dest, *src;
      dest = (Value *) iptr;
      src = ins.getOperand(0);
      if (acp.find(src) != acp.end()) {
        acp[dest] = acp[src];
        to_remove.push_back(iptr);
      }
    } else {
    // replace operands that are copies if in acp table (unless its a load)
      for (i = 0; i < ins.getNumOperands(); i++) {
        Value *op = ins.getOperand(i);
        if (acp.find(op) != acp.end()) {
          ins.setOperand(i, acp[op]);
        }
      }
    }

    for (Instruction *i : to_remove) {
      i->removeFromParent();
    }

    // add store instructions to the ACP table
    // if (isa<StoreInst>(iptr)) {
    //   Value *dest = ins.getOperand(1);
    //   Value *src = ins.getOperand(0);
    //   // ins.print(errs());
    //   // errs() << " src: " << src << " dest: " << dest << "\n";
    //   for (i = 0; i < ins.getNumOperands(); i++) {
    //     Value *op = ins.getOperand(i);
    //     if (acp.find(op) != acp.end()) {
    //       ins.setOperand(i, acp[op]);
    //     }
    //   }

    //   if (acp.find(src) != acp.end()) {
    //     // if src is in acp as a dest, store the src of that dest
    //     acp[dest] = acp[src];
    //   } else if (acp.find(dest) != acp.end()) {
    //     acp.erase(dest);
    //   } else {
    //     // otherwise just store the src
    //     acp[dest] = src;
    //   }
    // } else if (isa<LoadInst>(iptr)) {
    //   Value *src = ins.getOperand(0);
    //   // ins.print(errs());
    //   // errs() << " src: " << src << " dest: " << (Value*)iptr << "\n";
    //   if (acp.find(src) != acp.end()) {
    //     // if src is in acp as a dest, store the src of that dest
    //     acp[(Value *)iptr] = acp[src];
    //     // remove instruction
    //     // ins.eraseFromParent();
    //   } else {
    //     // otherwise just store the src
    //     acp[(Value *)iptr] = src;
    //   }
    // } else {
    //   for (i = 0; i < ins.getNumOperands(); i++) {
    //     Value *op = ins.getOperand(i);
    //     if (acp.find(op) != acp.end()) {
    //       ins.setOperand(i, acp[op]);
    //     }
    //   }
    // }
  }

  // go through and remove all loads
  // for (auto it = bb.begin(); it != bb.end();) {
  //   Instruction &ins = *it;
  //   ++it;  // Advance iterator before potentially erasing 'ins'

  //   if (isa<LoadInst>(ins)) {
  //     // Value *src = ins.getOperand(0);
  //     // if (acp.find(src) != acp.end() && src->use_empty()) {
  //     //   ins.eraseFromParent();
  //     // }
  //     
  //     if (ins.use_empty()) {
  //       ins.eraseFromParent();
  //     }
  //   }
  // }
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
void CopyPropagation::localCopyPropagation( Function &F )
{
    ACPTable acp;

    for (BasicBlock &bb : F) {
      propagateCopies(bb, acp);  
    }
    
    // debug
    if (verbose)
    {
        errs() << "post local" << "\n" << (*(&F)) << "\n";
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
void CopyPropagation::globalCopyPropagation( Function &F )
{
    DataFlowAnalysis *dfa;
    dfa = new DataFlowAnalysis(F);
    
    // implement global copy propagation here

    if (verbose)
    {
        errs() << "post global" << "\n" << (*(&F)) << "\n";
    }
}


/*
 * addCopy is a helper routine for initCopyIdxs. It updates state information
 * to record the index of a single copy instruction
 */
void DataFlowAnalysis::addCopy(Value* v)
{
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
void DataFlowAnalysis::initCopyIdxs(Function &F)
{
  // add copy for all function args
  for(auto ai = F.arg_begin(); ai != F.arg_end(); ai++) {
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
void DataFlowAnalysis::initCOPYAndKILLSets(Function &F)
{
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
 * Use set operations on the appropriate BitVector to create CPIn and CPOut.
 */
void DataFlowAnalysis::initCPInAndCPOutSets(Function &F)
{
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
void DataFlowAnalysis::initACPs()
{
}

ACPTable &DataFlowAnalysis::getACP(BasicBlock &bb)
{
    return bb_info[(&bb)]->ACP;
}


void DataFlowAnalysis::printCopyIdxs()
{
    errs() << "copy_idx:" << "\n";
    for ( auto it = copy_idx.begin(); it != copy_idx.end(); ++it )
    {
        errs() << "  " << format("%-3d", it->second)
               << " --> " << *( it->first ) << "\n";
    }
    errs() << "\n";
}

void DataFlowAnalysis::printDFA()
{
    unsigned int i;

    // used for formatting
    std::string str;
    llvm::raw_string_ostream rso( str );

    for ( auto it = bb_info.begin(); it != bb_info.end(); ++it )
    {
        BasicBlockInfo *bbi = bb_info[it->first];

        errs() << "BB ";
        it->first->printAsOperand(errs(), false);
        errs() << "\n";

        errs() << "  CPIn  ";
        for ( i = 0; i < bbi->CPIn.size(); i++ )
        {
            errs() << bbi->CPIn[i] << ' ';
        }
        errs() << "\n";

        errs() << "  CPOut ";
        for ( i = 0; i < bbi->CPOut.size(); i++ )
        {
            errs() << bbi->CPOut[i] << ' ';
        }
        errs() << "\n";

        errs() << "  COPY  ";
        for ( i = 0; i < bbi->COPY.size(); i++ )
        {
            errs() << bbi->COPY[i] << ' ';
        }
        errs() << "\n";

        errs() << "  KILL  ";
        for ( i = 0; i < bbi->KILL.size(); i++ )
        {
            errs() << bbi->KILL[i] << ' ';
        }
        errs() << "\n";

        errs() << "  ACP:" << "\n";
        for ( auto it = bbi->ACP.begin(); it != bbi->ACP.end(); ++it )
        {
            rso << *( it->first );
            errs() << "  " << format("%-30s", rso.str().c_str()) << "==  "
                   << *( it->second ) << "\n";
            str.clear();
        }
        errs() << "\n" << "\n";
    }
}

/*
 * DataFlowAnalysis constructs the data flow analysis for the function F.
 *
 * You will not need to modify this routine.
 */
DataFlowAnalysis::DataFlowAnalysis( Function &F )
{
    initCopyIdxs(F);
    initCOPYAndKILLSets(F);
    initCPInAndCPOutSets(F);
    initACPs();

    if (CopyPropagation::verbose) {
        errs() << "post DFA" << "\n";
        printCopyIdxs();
        printDFA();
    }
}

