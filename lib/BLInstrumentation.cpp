/*
 * =====================================================================================
 *
 *       Filename:  BL.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Thursday, May 22, 2014 09:43:01 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author: 
 *        Company:
 *
 * =====================================================================================
 */
#include "../include/BLInstrumentation.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Twine.h"
#include <cstdint>
#include <iostream>

namespace pathTracing {

BLDag* generateHPPPathNumberingDag(llvm::Function* func) {
	BLDag* dag = new BLDag(func);
	dag->init();
	dag->calculatePathNumbers();
	return dag;
}

BLDag* generateWPPDag(llvm::Function* func) {
	BLDag* dag = new BLDag(func);
	dag->initWPPDag();
	dag->calculatePathNumbers();
	return dag;
}
BLWPPRecoveryDag* generateWPPPathNumberingDag(llvm::Function* func) {
	BLWPPRecoveryDag* dag = new BLWPPRecoveryDag(func);
	dag->initWPPDag();
	dag->calculatePathNumbers();
	dag->calculateOriginalBLPathNumbers();
	return dag;
}

BLDag* generateHPPPathInstrumentationDag(llvm::Function* func) {
	BLDag* dag = new BLDag(func);
	dag->init();
	dag->calculatePathNumbers();
	dag->calculateSpanningTree();
	dag->calculateChordIncrements();
	dag->pushInitialization();
	dag->labelBLPathEnd();
	dag->unlinkPhony();
	return dag;
}

BLDag* generateWPPPathInstrumentationDag(llvm::Function* func) {
	BLDag* dag = new BLDag(func);
	dag->initWPPDag();
	dag->calculatePathNumbers();
	dag->calculateSpanningTree();
	dag->calculateChordIncrements();
	dag->pushInitialization();
	dag->labelBLPathEnd();
	dag->unlinkPhony();
	return dag;
}

llvm::Constant* normalBLPathProbe = NULL;
llvm::Constant* path2IndeterCSProbe = NULL;
llvm::Constant* path2UntrackCSProbe = NULL;
llvm::Constant* path2TrackCSProbe = NULL;

void setNormalBLPathProbe(llvm::Constant* p) {
	normalBLPathProbe = p;
}
void setPath2IndeterCSProbe(llvm::Constant* p) {
	path2IndeterCSProbe = p;
}
void setPath2UntrackCSProbe(llvm::Constant* p) {
	path2UntrackCSProbe = p;
}
void setPath2TrackCSProbe(llvm::Constant* p) {
	path2TrackCSProbe = p;
}

//-----------------------------------------------------------------------------
//-------------functions used by placeBLPathInstrumentation--------------------
//void splitLandingPadForCriticalEdge(llvm::BasicBlock* OrigBB);
bool splitCritical(BLEdge* edge, BLDag* dag, llvm::Pass* pass);
void instrumentEdge(BLEdge* edge, llvm::LLVMContext* context);
void placeEdgeInstrumentation(BLNode* instrumentNode, BLEdge* processedEdge,
		bool atBeginning, llvm::BasicBlock::iterator insertPoint,
		llvm::Constant* blPathProbe, llvm::LLVMContext* context);
void pushValueIntoNode(BLNode* source, BLNode* target, llvm::LLVMContext* context);
void preparePHI(BLNode* node, llvm::LLVMContext* context);
void pushValueIntoPHI(BLNode* target, BLNode* source);
llvm::ConstantInt* createIncrementConstant(long incr, int bitsize, llvm::LLVMContext* context);
llvm::ConstantInt* createIncrementConstant(BLEdge* edge, llvm::LLVMContext* context);
//-----------------------------------------------------------------------------

// Given a CFG with each edge labeled, instrument each edge in depth-first order
void placeBLPathInstrumentation(BLDag* dag, llvm::LLVMContext* context, llvm::Pass* pass) {

	std::stack<BLEdge*> edgeStack;
	BLEdge* exitRootEdge = (BLEdge*) dag->getExitRootEdge();
	// exit->root and root->entry_block edges are not chord edges, so no instrumentation will
	// be placed on them
	assert(!exitRootEdge->isChordEdge());
	exitRootEdge->setHasInstrumentation(true);
	BLNode* root = dag->getRoot();
	assert(root->getNumberSuccEdges() == 1
			&& "Root node does not follow the rule that it has one and only one successor edge");

	dag->splitCriticalLandingPads();

	edgeStack.push(*(root->succBegin()));
	assert(!(*(root->succBegin()))->isChordEdge());
	while (edgeStack.size() > 0) {
		BLEdge* edge = edgeStack.top();
		edgeStack.pop();
		// due to the existence of loop, some edges in stack may have already been instrumented
		if (edge->hasInstrumentation())
			continue;
		// Mark the edge as instrumented
		edge->setHasInstrumentation(true);

		DEBUG(llvm::dbgs() << "\nInstrumenting edge: " << (*edge) << "\n");

		// create a new node for this edge's instrumentation if it is a critical edge
		splitCritical(edge, dag, pass);
		// place instrumentation on that edge
		instrumentEdge(edge, context);

		BLNode* targetNode = edge->getTarget();
		// Add all the successors
		for( BLEdgeIterator next = targetNode->succBegin(),
				end = targetNode->succEnd(); next != end; next++ ) {
			// So long as it is un-instrumented, push it to the stack
			if( !((BLEdge*)(*next))->hasInstrumentation() )
				edgeStack.push((BLEdge*)(*next));	
			else
				DEBUG(llvm::dbgs() << "  Edge " << *(BLEdge*)(*next)
						<< " already instrumented.\n");
		}
	}
}

void instrumentEdge(BLEdge* edge, llvm::LLVMContext* context) {
	BLNode* sourceNode = edge->getSource();
	BLNode* targetNode = edge->getTarget();
	BLNode* instrumentNode = NULL;
	BLNode* nextSourceNode = NULL;

	bool atBeginning = false;

	// Source node has only 1 successor so any information can be simply
	// inserted in to it without splitting
	if( sourceNode->getBlock() && sourceNode->getNumberSuccEdges() <= 1) {
		DEBUG(llvm::dbgs() << "  Potential instructions to be placed in: "
				<< sourceNode->getName() << " (at end)\n");
		instrumentNode = sourceNode;
		nextSourceNode = targetNode; // ... since we never made any new nodes
	}
	// The target node only has one predecessor, so we can safely insert edge
	// instrumentation into it. If there was splitting, it must have been
	// successful.
	else if( targetNode->getBlock() && targetNode->getNumberPredEdges() == 1 ) {
		DEBUG(llvm::dbgs() << "  Potential instructions to be placed in: "
				<< targetNode->getName() << " (at beginning)\n");
		pushValueIntoNode(sourceNode, targetNode, context);
		instrumentNode = targetNode;
		nextSourceNode = NULL; // ... otherwise we'll just keep splitting
		atBeginning = true;
	}
	// Somehow, splitting must have failed.
	else {
		assert(!"Instrumenting could not split a critical edge.\n");
		llvm::errs() << "Instrumenting could not split a critical edge.\n";
		return;
	}

	llvm::BasicBlock::iterator insertPoint = atBeginning ?
		instrumentNode->getBlock()->getFirstInsertionPt() :
		instrumentNode->getBlock()->getTerminator();

	// Insert instrumentation if this is a back or split edge
	if( edge->getType() == BLEdge::BACKEDGE ||
			edge->getType() == BLEdge::SPLITEDGE ){
		assert(edge->getType() != BLEdge::BACKEDGE || atBeginning == false);
		BLEdge* top = edge->getPhonyRoot();
		BLEdge* bottom = edge->getPhonyExit();

		placeEdgeInstrumentation(instrumentNode, bottom, atBeginning, insertPoint, normalBLPathProbe, context);
		placeEdgeInstrumentation(instrumentNode, top, atBeginning, insertPoint, normalBLPathProbe, context);
	} else if (edge->getType() == BLEdge::CALLEDGE) {
		BLEdge* top = edge->getPhonyRoot();
		BLEdge* bottom = edge->getPhonyExit();
		// determine the type of call site
		bool isIndeterCS = false;
		bool isUntrackCS = false;

		// the first instruction of the target node must be a CallInst or InvokeInst
		//	which is guaranteed by BLDag::splitCallBlocks()
		llvm::Instruction* inst = edge->getTarget()->getBlock()->getFirstNonPHIOrDbgOrLifetime();
		if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst)) {
			llvm::Function* calledFunc = callInst->getCalledFunction();
			if (!calledFunc) {
				// if indirect function call through function pointer
				isIndeterCS = true;
			}else if (calledFunc->isDeclaration()) {
				// if function call is declared
				// in default all declared functions are not tracked
				isUntrackCS = true;
			}
		} else if (llvm::InvokeInst* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(inst)) {
			llvm::Function* calledFunc = invokeInst->getCalledFunction();
			if (!calledFunc) {
				// if indirect function call through function pointer
				isIndeterCS = true;
			} else if (calledFunc->isDeclaration()) {
				isUntrackCS = true;
			}
		}
		if (isIndeterCS) {
			// for call edge on indeterministic(indirect) call site
			placeEdgeInstrumentation(instrumentNode, bottom, atBeginning, insertPoint, path2IndeterCSProbe, context);
		} else if (isUntrackCS) {
			// for call edge on untracked call site
			placeEdgeInstrumentation(instrumentNode, bottom, atBeginning, insertPoint, path2UntrackCSProbe, context);
		} else {
			// normal call edge
			placeEdgeInstrumentation(instrumentNode, bottom, atBeginning, insertPoint, path2TrackCSProbe, context);
		}
		placeEdgeInstrumentation(instrumentNode, top, atBeginning, insertPoint, normalBLPathProbe, context);
	} else {
		// Insert instrumentation if this is a normal edge
		placeEdgeInstrumentation(instrumentNode, edge, atBeginning, insertPoint, normalBLPathProbe, context);
	}

	// Push it along
	if (nextSourceNode && instrumentNode->getEndingPathNumber()){
		pushValueIntoNode(instrumentNode, nextSourceNode, context);
	}
}

// If this edge is a critical edge, then inserts a node at this edge.
// This edge becomes the first edge, and a new BLEdge is created.
// Returns true if the edge was split
// the reason to split a critical edge is that instrumentation on edge must
// be instrumented on either the end of its source node or the begining of its
// target node and assure that the instrumented code is exercised only when
// control flow goes through this edge
bool splitCritical(BLEdge* edge, BLDag* dag, llvm::Pass* pass) {
  unsigned succNum = edge->getSuccessorNumber();
  BLNode* sourceNode = edge->getSource();
  BLNode* targetNode = edge->getTarget();
  llvm::BasicBlock* sourceBlock = sourceNode->getBlock();
  llvm::BasicBlock* targetBlock = targetNode->getBlock();

  if(sourceBlock == NULL || targetBlock == NULL
     || sourceNode->getNumberSuccEdges() <= 1
     || targetNode->getNumberPredEdges() == 1 ) {
	  return false;
  }

  llvm::TerminatorInst* terminator = sourceBlock->getTerminator();

  if(SplitCriticalEdge(terminator, succNum, pass, false)) {
	  llvm::BasicBlock* newBlock = terminator->getSuccessor(succNum);
	  dag->splitUpdate(edge, newBlock);
	  return true;
//  } else if(targetBlock->isLandingPad()) {
//	  // if the target block is a landingpad, then SplitCriticalEdge will fail
//	  splitLandingPadForCriticalEdge(targetBlock);
//	  // splitUpdate
  } else {
	  assert("Split Critical Fails\n");
	  return false;
  }
}

void placeEdgeInstrumentation(BLNode* instrumentNode, BLEdge* processedEdge,
		bool atBeginning, llvm::BasicBlock::iterator insertPoint,
		llvm::Constant* blPathProbe, llvm::LLVMContext* context) {
	
	assert(blPathProbe && "function probe to be instrumented is NULL!");
	if( processedEdge->isInitialization() ) { // initialize path number
		instrumentNode->setEndingPathNumber(createIncrementConstant(processedEdge, context));
	} else if( processedEdge->getIncrement() )       {// increment path number
		llvm::Value* newpn =
			llvm::BinaryOperator::Create(llvm::Instruction::Add,
					instrumentNode->getStartingPathNumber(),
					createIncrementConstant(processedEdge, context),
					"pathNumber", insertPoint);

		instrumentNode->setEndingPathNumber(newpn);
		if( atBeginning ){
			instrumentNode->setStartingPathNumber(newpn);
		}
	}

	// if the processedEdge end with the EXIT node, it marks the end of the bl path
	if( processedEdge->isBLPathEnd() ) {
		std::vector<llvm::Value*> args(1);
		args[0] = instrumentNode->getEndingPathNumber();
		llvm::CallInst::Create(blPathProbe, args, "", insertPoint);
		instrumentNode->setEndingPathNumber(NULL);
	}
}

// Inserts source's pathNumber Value* into target.  Target may or may not
// have multiple predecessors, and may or may not have its phiNode
// initalized.
void pushValueIntoNode(BLNode* source, BLNode* target, llvm::LLVMContext* context) {
  if(target->getBlock() == NULL)
    return;

  if(target->getNumberPredEdges() <= 1) {
    assert(target->getStartingPathNumber() == NULL &&
           "Target already has path number");
    target->setStartingPathNumber(source->getEndingPathNumber());
    target->setEndingPathNumber(source->getEndingPathNumber());
    DEBUG(llvm::dbgs() << "  Passing path number"
          << (source->getEndingPathNumber() ? "" : " (null)")
          << " value through.\n");
  } else {
    if(target->getPathPHI() == NULL) {
      DEBUG(llvm::dbgs() << "  Initializing PHI node for block '"
            << target->getName() << "'\n");
      preparePHI(target, context);
    }
    pushValueIntoPHI(target, source);
    DEBUG(llvm::dbgs() << "  Passing number value into PHI for block '"
          << target->getName() << "'\n");
  }
}

// A PHINode is created in the node, and its values initialized to -1U.
void preparePHI(BLNode* node, llvm::LLVMContext* context) {
	llvm::BasicBlock* block = node->getBlock();
	llvm::BasicBlock::iterator insertPoint = block->getFirstInsertionPt();
	llvm::pred_iterator PB = llvm::pred_begin(node->getBlock()),
          PE = llvm::pred_end(node->getBlock());
	assert(std::distance(PB, PE) > 1);
	llvm::PHINode* phi = llvm::PHINode::Create(llvm::Type::getInt64Ty(*context),
                                 std::distance(PB, PE), "pathNumber",
                                 insertPoint );
	node->setPathPHI(phi);
	node->setStartingPathNumber(phi);
	node->setEndingPathNumber(phi);

	for(llvm::pred_iterator predIt = PB; predIt != PE; predIt++) {
		llvm::BasicBlock* pred = (*predIt);

    if(pred != NULL)
      phi->addIncoming(createIncrementConstant(-1, 64, context), pred);
  }
}

// Inserts source's pathNumber Value* into the appropriate slot of
// target's phiNode.
void pushValueIntoPHI(BLNode* target, BLNode* source) {
	llvm::PHINode* phi = target->getPathPHI();
	assert(phi != NULL && "  Tried to push value into node with PHI, but node"
         " actually had no PHI.");
	phi->removeIncomingValue(source->getBlock(), false);
	phi->addIncoming(source->getEndingPathNumber(), source->getBlock());
}

// Creates an increment constant representing incr.
llvm::ConstantInt* createIncrementConstant(long incr, int bitsize, llvm::LLVMContext* context) {
  return(llvm::ConstantInt::get(llvm::IntegerType::get(*context, bitsize), incr));
}

// Creates an increment constant representing the value in
// edge->getIncrement().
llvm::ConstantInt* createIncrementConstant(BLEdge* edge, llvm::LLVMContext* context) {
  return(createIncrementConstant(edge->getIncrement(), 64, context));
}
}//end of namespace pathTracing
