//===- BLDag.cpp --------------------------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Ball-Larus path numbers uniquely identify paths through a directed acyclic
// graph (DAG) [Ball96].  For a CFG backedges are removed and replaced by phony
// edges to obtain a DAG, and thus the unique path numbers [Ball96].
//
// The purpose of this analysis is to enumerate the edges in a CFG in order
// to obtain paths from path numbers in a convenient manner.  As described in
// [Ball96] edges can be enumerated such that given a path number by following
// the CFG and updating the path number, the path is obtained.
//
// [Ball96]
//  T. Ball and J. R. Larus. "Efficient Path Profiling."
//  International Symposium on Microarchitecture, pages 46-57, 1996.
//  http://portal.acm.org/citation.cfm?id=243857
//
//===----------------------------------------------------------------------===//
//#define DEBUG_TYPE "ball-larus-numbering"

#include "../include/BLDag.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/ADT/DenseMap.h"

#define PATH_SPLIT_THRESHHOLD 100000000

#include <stack>

using namespace pathTracing;

// BLDag constructor initializes a DAG for the given Function.
BLDag::BLDag(llvm::Function* F) :
    _root(NULL), _exit(NULL), _function(F) {}

// Initialization that requires virtual functions which are not fully
// functional in the constructor.
void BLDag::init() {
  BLBlockNodeMap inDag;
  std::stack<BLNode*> dfsStack;

//  _root = addNode(&(_function.getEntryBlock()));
  _root = addNode(NULL);
  BLNode* entryNode = addNode(&(_function->getEntryBlock()));
  addEdge(_root, entryNode);
  _exit = addNode(NULL);
  // start search from root
//  dfsStack.push(getRoot());
  dfsStack.push(entryNode);
  // dfs to add each bb into the dag
  while(dfsStack.size())
    buildNode(inDag, dfsStack);

  // put in the final edge
  addEdge(getExit(),getRoot());
  BLEdgeIterator erEdge = getExit()->succBegin();
  (*erEdge)->setType(BLEdge::PHONYEDGE);
  processPhonyEdgesForBackEdges();
}

void BLDag::initWPPDag() {
	splitCallBlocks();
	init();
	processPhonyEdgesForCallEdges();
}

// split the basic block at all function calls except intrinsic functions
void BLDag::splitCallBlocks() {
	std::vector<llvm::BasicBlock*> BBWithCallInst;
	for (llvm::Function::iterator bb = _function->begin(), end = _function->end(); bb != end; ++bb) {
		for (llvm::BasicBlock::iterator inst = bb->begin(), end = bb->end(); inst != end; ++inst) {
			if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst)) {
				llvm::Function* func = callInst->getCalledFunction();
				// if this function call is not intrinsic
				if (!func || !func->isIntrinsic()) {
					BBWithCallInst.push_back(bb);
					break;
				}
			}else if (llvm::isa<llvm::InvokeInst>(inst)) {
				BBWithCallInst.push_back(bb);
				break;
			}
		}
	}
	for(std::vector<llvm::BasicBlock*>::iterator it = BBWithCallInst.begin(); 
			it != BBWithCallInst.end(); ++it) {
		llvm::BasicBlock* bb = *it;
		llvm::BasicBlock::iterator inst = bb->begin();
		while(inst != bb->end()) {
			if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst)) {
				llvm::Function* func = callInst->getCalledFunction();
				// if this function call is not intrinsic
				if (!func || !func->isIntrinsic()) {
					// split the original basic block & set the on process
					// basic block to the newly generated one
					bb = bb->splitBasicBlock(inst);
					// set the inst to the begining of the new basic block
					// the inst should be the call instruction used to split the basic block lately
					inst = bb->begin();
				}
			}else if (llvm::isa<llvm::InvokeInst>(inst)) {
				
				bb = bb->splitBasicBlock(inst);
				inst = bb->begin();
			}
			++inst;
		}
	}
}

// Frees all memory associated with the DAG.
BLDag::~BLDag() {
  for(BLEdgeIterator edge = _edges.begin(), end = _edges.end(); edge != end;
      ++edge)
    delete (*edge);

  for(BLNodeIterator node = _nodes.begin(), end = _nodes.end(); node != end;
      ++node)
    delete (*node);
}

// Calculate the path numbers by assigning edge increments as prescribed
// in Ball-Larus path profiling.
void BLDag::calculatePathNumbers() {
  uint32_t currentSplitThreshold = UINT_MAX;
  BLNode* node;
  std::queue<BLNode*> bfsQueue;
  bfsQueue.push(getExit());

  while(bfsQueue.size() > 0) {
    node = bfsQueue.front();

    DEBUG(llvm::dbgs() << "calculatePathNumbers on " << node->getName() << "\n");

    bfsQueue.pop();
    if (node->getNumberPaths() != 0) {
      // the node has been processed, skip it
      continue;
    }
    uint64_t numOfPaths = calculatePathNumbersFrom(node);
    if (node == getRoot()) {
      if (numOfPaths > UINT_MAX) {
        // the current path split threshold cannot assure pathIDs not overflowing 32bits 
        // placeholders, re-split
        currentSplitThreshold = currentSplitThreshold / 2;
        DEBUG(llvm::dbgs() << "Current split threshold fails, resplit with threshold " 
            << currentSplitThreshold << '\n');
        // clear the current queue
        std::queue<BLNode*>().swap(bfsQueue);
        for (BLNodeIterator nodeIterator = _nodes.begin(), end = _nodes.end();
            nodeIterator != end; ++nodeIterator) {
          BLNode* candidateNode = (*nodeIterator);
          // for all nodes whose numOfPaths > currentSplitThreshold and root (because it may already overflow)
          if (candidateNode == getRoot() || candidateNode->getNumberPaths() > currentSplitThreshold) {
            candidateNode->setNumberPaths(0);
            bfsQueue.push(candidateNode);
          }
        }
      }
    } else {
      // Check for DAG splitting
      if (numOfPaths > currentSplitThreshold) {
        splitNode(node);
        calculatePathNumbersFrom(node);
      }
      if (node->getNumberPaths() != 0) {
        for(BLEdgeIterator pred = node->predBegin(), end = node->predEnd();
            pred != end; pred++) {
          if( (*pred)->getType() == BLEdge::BACKEDGE ||
              (*pred)->getType() == BLEdge::SPLITEDGE ||
              (*pred)->getType() == BLEdge::CALLEDGE ) {
            continue;
          }
          BLNode* nextNode = (*pred)->getSource();
          if(nextNode->getNumberPaths() == 0) {
            bfsQueue.push(nextNode);
          }
        }
      }
    }
  }

  DEBUG(llvm::dbgs() << "\tNumber of paths: " << getRoot()->getNumberPaths() << "\n");
}

// Returns the number of paths for the Dag.
unsigned BLDag::getNumberOfPaths() const{
  return(getRoot()->getNumberPaths());
}

// Returns the root (i.e. entry) node for the DAG.
BLNode* BLDag::getRoot() {
  return _root;
}

const BLNode* BLDag::getRoot() const{
  return _root;
}

// Returns the exit node for the DAG.
BLNode* BLDag::getExit() {
  return _exit;
}

const BLNode* BLDag::getExit() const{
  return _exit;
}

// Returns the function for the DAG.
llvm::Function* BLDag::getFunction() {
  return(_function);
}

const llvm::Function* BLDag::getFunction() const{
  return(_function);
}

// Clears the node colors.
void BLDag::clearColors(BLNode::NodeColor color) {
  for (BLNodeIterator nodeIt = _nodes.begin(); nodeIt != _nodes.end(); nodeIt++)
    (*nodeIt)->setColor(color);
}

// Processes one node and its imediate edges for building the DAG.
void BLDag::buildNode(BLBlockNodeMap& inDag, BLNodeStack& dfsStack) {
  BLNode* currentNode = dfsStack.top();
  llvm::BasicBlock* currentBlock = currentNode->getBlock();

  if(currentNode->getColor() != BLNode::WHITE) {
    // we have already visited this node
    dfsStack.pop();
    currentNode->setColor(BLNode::BLACK);
  } else {
	llvm::TerminatorInst* terminator = currentNode->getBlock()->getTerminator();
    if(llvm::isa<llvm::ReturnInst>(terminator) || llvm::isa<llvm::UnreachableInst>(terminator) ||
       llvm::isa<llvm::ResumeInst>(terminator))
      addEdge(currentNode, getExit());

    currentNode->setColor(BLNode::GRAY);
    inDag[currentBlock] = currentNode;

    // iterate through this node's successors
    for(llvm::succ_iterator successor = llvm::succ_begin(currentBlock),
          succEnd = llvm::succ_end(currentBlock); successor != succEnd; ++successor ) {
      llvm::BasicBlock* succBB = *successor;

      buildEdge(inDag, dfsStack, currentNode, succBB);
    }
  }
}

// Process an edge in the CFG for DAG building.
void BLDag::buildEdge(BLBlockNodeMap& inDag, std::stack<BLNode*>&
                             dfsStack, BLNode* currentNode, llvm::BasicBlock* succBB) {
  BLNode* succNode = inDag[succBB];

  if(succNode && succNode->getColor() == BLNode::BLACK) {
    // visited node and forward edge
    addEdge(currentNode, succNode);
  } else if(succNode && succNode->getColor() == BLNode::GRAY) {
    // visited node and back edge
    DEBUG(llvm::dbgs() << "Backedge detected.\n");
//    addBackedge(currentNode, succNode);
	BLEdge* backEdge = addEdge(currentNode, succNode);
	backEdge->setType(BLEdge::BACKEDGE);
  } else {
    BLNode* childNode;
    // not visited node and forward edge
    if(succNode) // an unvisited node that is child of a gray node
      childNode = succNode;
    else { // an unvisited node that is a child of a an unvisted node
      childNode = addNode(succBB);
      inDag[succBB] = childNode;
    }
    addEdge(currentNode, childNode);
    dfsStack.push(childNode);
  }
}

// The weight on each edge is the increment required along any path that
// contains that edge.
uint64_t BLDag::calculatePathNumbersFrom(BLNode* node) {
  if(node == getExit()) {
    // The Exit node must be base case
    node->setNumberPaths(1);
    return 1;
  } else {
    uint64_t sumPaths = 0;
    BLNode* succNode;

    for(BLEdgeIterator succ = node->succBegin(), end = node->succEnd();
        succ != end; succ++) {
      if( (*succ)->getType() == BLEdge::BACKEDGE ||
          (*succ)->getType() == BLEdge::SPLITEDGE ||
		      (*succ)->getType() == BLEdge::CALLEDGE ) {
        continue;
      }

      (*succ)->setWeight(sumPaths);
      succNode = (*succ)->getTarget();

      if( !succNode->getNumberPaths() ) {
        return 0;
      }
      sumPaths += succNode->getNumberPaths();
    }

    node->setNumberPaths(sumPaths);
    return sumPaths;
  }
}

BLNode* BLDag::createNode(llvm::BasicBlock* BB) {
  return( new BLNode(BB) );
}

BLEdge* BLDag::createEdge(BLNode* source, BLNode* target) {
  return( new BLEdge(source, target) );
}

// Proxy to node's constructor.  Updates the DAG state.
BLNode* BLDag::addNode(llvm::BasicBlock* BB) {
  BLNode* newNode = createNode(BB);
  _nodes.push_back(newNode);
  return( newNode );
}

// Proxy to edge's constructor. Updates the DAG state.
BLEdge* BLDag::addEdge(BLNode* source, BLNode* target) {
  BLEdge* newEdge = createEdge(source, target);
  _edges.push_back(newEdge);
  source->addSuccEdge(newEdge);
  target->addPredEdge(newEdge);
  return(newEdge);
}

void BLDag::processPhonyEdgesForBackEdges() {
	BLEdgeVector original_edges = _edges;
	// process back edges
	for(BLEdgeIterator edge= original_edges.begin(), end = original_edges.end(); edge != end; ++edge) {
		BLEdge* backEdge = (BLEdge*)(*edge);
		assert(backEdge && "iterator edge is null");
		if (backEdge->getType() == BLEdge::BACKEDGE) {
			backEdge->setPhonyRoot(addPhonyEdge(getRoot(), backEdge->getTarget()));
			backEdge->setPhonyExit(addPhonyEdge(backEdge->getSource(), getExit()));
		}
	}
}

void BLDag::processPhonyEdgesForCallEdges() {
	// process call edges
	for (BLNodeIterator node = _nodes.begin(), end = _nodes.end(); node != end; ++node) {
		llvm::BasicBlock* bb = (*node)->getBlock();
		if (bb != NULL) {
			for (llvm::BasicBlock::iterator inst = bb->begin(), end = bb->end(); inst != end; ++inst) {
				if (llvm::isa<llvm::InvokeInst>(inst)) {
					splitCallNode(*node);
				} else if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst)) {
					llvm::Function* func = callInst->getCalledFunction();
					if (!func || !func->isIntrinsic()) {
						splitCallNode(*node);
					}
				}
			}
		}
	}
}

void BLDag::splitCallNode(BLNode* node) {
	assert(node->getNumberPredEdges() == 1 && "Call node should have only one predecessor");
	BLEdge* callEdge = *(node->predBegin());

	assert(callEdge->getType() == BLEdge::NORMAL);
	assert(callEdge->getSource() != getRoot());
	assert(callEdge->getSource()->getNumberSuccEdges() == 1);

	BLEdge* rootEdge = addPhonyEdge(getRoot(), node);
	BLEdge* exitEdge = addPhonyEdge(callEdge->getSource(), getExit());

	callEdge->setType(BLEdge::CALLEDGE);
	callEdge->setPhonyRoot(rootEdge);
	callEdge->setPhonyExit(exitEdge);
} 

BLEdge* BLDag::addPhonyEdge(BLNode* source, BLNode* target) {
//	bool edgeExist = false;
	for(BLEdgeIterator succ = source->succBegin(), end = source->succEnd();
			succ != end; succ++) {
		BLEdge* succEdge = (BLEdge*)(*succ);
		assert(succEdge && "succEdge is null");
		if (succEdge->getTarget() == target) {
//			edgeExist = true;
			return succEdge;
		}
	}
	BLEdge* newEdge = addEdge(source, target);
	newEdge->setType(BLEdge::PHONYEDGE);
	return newEdge;
}

void BLDag::splitNode(BLNode* node) {

	BLEdge* exitEdge = NULL;
	for( BLEdgeIterator succ = node->succBegin(), end = node->succEnd();
			succ != end; succ++ ) {
		if ( (*succ)->getTarget() == getExit() ) {
			exitEdge = (*succ);
			break;
		}
	}
	if (exitEdge == NULL) {
		exitEdge = addPhonyEdge(node, getExit());
	}
	// Iterate through each successor edge, adding phony edges
	for( BLEdgeIterator succ = node->succBegin(), end = node->succEnd();
			succ != end; succ++ ) {

		if( (*succ)->getType() == BLEdge::NORMAL && (*succ)->getTarget() != getExit()) {
      BLNode* succNode = (*succ)->getTarget();
      BLEdge* rootEdge = NULL;
      for (BLEdgeIterator pred = succNode->predBegin(), end = succNode->predEnd();
          pred != end; pred++) {
        if ((*pred)->getSource() == getRoot()) {
          rootEdge = (*pred);
//          DEBUG(llvm::dbgs() << "rootEdge exists: " << *rootEdge << "\n");
          break;
        }
      }
      if (rootEdge == NULL) {
        // create the new phony edge: root -> succ
        rootEdge = addPhonyEdge(getRoot(), (*succ)->getTarget());
      }

			// split on this edge and reference it's exit/root phony edges
			(*succ)->setType(BLEdge::SPLITEDGE);
			(*succ)->setPhonyRoot(rootEdge);
			(*succ)->setPhonyExit(exitEdge);
//			(*succ)->setWeight(0);
		}
	}
} 

// for trace recovery
// algorithm based on paper "Efficient Path Profiling" section 3.5
std::vector<const llvm::BasicBlock*>* BLDag::getPathBB(unsigned pathID) const{
	unsigned rValue = pathID;
	std::vector<const llvm::BasicBlock*>* BBSequence = new std::vector<const llvm::BasicBlock*>();
	const BLNode* currentProcessingNode = this->getRoot();
	while (currentProcessingNode != getExit()) {
		unsigned maxWeight = 0;
		const BLEdge* edgeInPath = NULL;
		for( BLEdgeConstIterator succ = currentProcessingNode->succBegin(), end = currentProcessingNode->succEnd();
				succ != end; succ++ ) {
			if( (*succ)->getType() == BLEdge::BACKEDGE ||
					(*succ)->getType() == BLEdge::SPLITEDGE)
				continue;
			unsigned edgeWeight = (*succ)->getWeight();
//			llvm::outs()<<"edgeWeight: "<<edgeWeight<<"\n";
			if (edgeWeight <= rValue && edgeWeight >= maxWeight) {
				maxWeight = edgeWeight;
				edgeInPath = (*succ);
			}
		}
		if (!edgeInPath) {
			llvm::errs()<< "The path "<< pathID << " of function " << this->getFunction()->getName() <<" is not found!"<<'\n';
			delete BBSequence;
			return NULL;
		}
		rValue -= edgeInPath->getWeight();
//		llvm::outs()<<"rValue: "<<rValue<<"\n";
		currentProcessingNode = edgeInPath->getTarget();
		if (currentProcessingNode != getExit()) {
			BBSequence->push_back(currentProcessingNode->getBlock());
		} else {
			if (rValue != 0) {
				llvm::errs()<< "The path "<< pathID << " is not found!"<<'\n';
				delete BBSequence;
				return NULL;
			}
		}
	}
	return BBSequence;
}

bool BLDag::isProcExitPath(unsigned pathID) const{
	unsigned rValue = pathID;
	const BLNode* currentProcessingNode = this->getRoot();
	while (currentProcessingNode != getExit()) {
		unsigned maxWeight = 0;
		const BLEdge* edgeInPath = NULL;
		for( BLEdgeConstIterator succ = currentProcessingNode->succBegin(), end = currentProcessingNode->succEnd();
				succ != end; succ++ ) {
			if( (*succ)->getType() == BLEdge::BACKEDGE ||
					(*succ)->getType() == BLEdge::SPLITEDGE)
				continue;
			unsigned edgeWeight = (*succ)->getWeight();
//			llvm::outs()<<"edgeWeight: "<<edgeWeight<<"\n";
			if (edgeWeight <= rValue && edgeWeight >= maxWeight) {
				maxWeight = edgeWeight;
				edgeInPath = (*succ);
			}
		}
		if (!edgeInPath) {
			llvm::errs()<< "The path "<< pathID << " of function " << this->getFunction()->getName() <<" is not found!"<<'\n';
			assert("path id not recognized");
		}
		rValue -= edgeInPath->getWeight();
		currentProcessingNode = edgeInPath->getTarget();
		if (currentProcessingNode == getExit()) {
			if (rValue != 0) {
				llvm::errs()<< "The path "<< pathID << " is not found!"<<'\n';
				assert("path id not recognized");
			} else {
				if (edgeInPath->getType() == BLEdge::PHONYEDGE) {
					return false;
				} else
					return true;
			}
		}
	}
	assert("path id not recognized");
}

int BLDag::getNodesNum() const{
	return _nodes.size();
}

int BLDag::getEdgesNum() const{
	return _edges.size();
}

// Returns the Exit->Root edge. This edge is required for creating
// directed cycles in the algorithm for moving instrumentation off of
// the spanning tree
const BLEdge* BLDag::getExitRootEdge() const{
  BLEdgeConstIterator erEdge = getExit()->succBegin();
  return(*erEdge);
}

BLEdge* BLDag::getExitRootEdge() {
  BLEdgeIterator erEdge = getExit()->succBegin();
  return(*erEdge);
}

// Calculates the increment for the chords, thereby removing
// instrumentation from the spanning tree edges. Implementation is based on
// the algorithm in Figure 4 of [Ball94]
void BLDag::calculateChordIncrements() {
  calculateChordIncrementsDfs(0, getRoot(), NULL);

  BLEdge* chord;
  for(BLEdgeIterator chordEdge = _chordEdges.begin(),
      end = _chordEdges.end(); chordEdge != end; chordEdge++) {
    chord = (BLEdge*) *chordEdge;
    chord->setIncrement(chord->getIncrement() + chord->getWeight());
  }
}

// Updates the state when an edge has been split
void BLDag::splitUpdate(BLEdge* formerEdge, llvm::BasicBlock* newBlock) {
  BLNode* oldTarget = formerEdge->getTarget();
  BLNode* newNode = addNode(newBlock);
  formerEdge->setTarget(newNode);
  newNode->addPredEdge(formerEdge);

  DEBUG(llvm::dbgs() << "  Edge split: " << *formerEdge << "\n");

  oldTarget->removePredEdge(formerEdge);
  BLEdge* newEdge = addEdge(newNode, oldTarget);

  if( formerEdge->getType() == BLEdge::BACKEDGE ||
         formerEdge->getType() == BLEdge::SPLITEDGE ||
		 formerEdge->getType() == BLEdge::CALLEDGE ) {
                newEdge->setType(formerEdge->getType());
    newEdge->setPhonyRoot(formerEdge->getPhonyRoot());
    newEdge->setPhonyExit(formerEdge->getPhonyExit());
    formerEdge->setType(BLEdge::NORMAL);
                formerEdge->setPhonyRoot(NULL);
    formerEdge->setPhonyExit(NULL);
  }
}

void splitLandingPadForCriticalEdge(BLNode* splitLPadNode, llvm::DenseMap<BLEdge*, llvm::BasicBlock*>& edgeBBMap) {
	llvm::BasicBlock* OrigBB = splitLPadNode->getBlock();
	assert(OrigBB->isLandingPad() && "Trying to split a non-landing pad!");
	assert(!OrigBB->getUniquePredecessor() && "Split a landing pad with unique predecessor!");
	llvm::LandingPadInst *lPad = OrigBB->getLandingPadInst();
	llvm::SmallVector<BLEdge*, 8> predEdges;
	llvm::SmallVector<llvm::BasicBlock*, 8> predBBs;
	llvm::SmallVector<llvm::BasicBlock*, 8> newBBs;

	for (BLEdgeIterator i = splitLPadNode->predBegin(), e = splitLPadNode->predEnd(); i != e; ++i) {
		predEdges.push_back(*i);
		predBBs.push_back((*i)->getSource()->getBlock());
	}
	for (llvm::SmallVectorImpl<llvm::BasicBlock*>::iterator i = predBBs.begin(),
			e = predBBs.end(); i != e; ++i) {
		llvm::BasicBlock* pred = *i;
		// for each predecessor of OrigBB, create a new basic block
		std::string newBBName = OrigBB->getName().str().append(".split.").append(std::to_string(newBBs.size()));
		llvm::BasicBlock* newBB = llvm::BasicBlock::Create(OrigBB->getContext(),
				newBBName.c_str(), OrigBB->getParent(), OrigBB);
		newBBs.push_back(newBB);
		// for each new basic block, let it has an indirect branch to the OrigBB
		llvm::BranchInst::Create(OrigBB, newBB);
		// let the predecessor points to the new basic block instead of OrigBB
		pred->getTerminator()->replaceUsesOfWith(OrigBB, newBB);
		// clone the landingpad inst of OrigBB into the new basic block
		llvm::Instruction *cloneLPad = lPad->clone();
		newBB->getInstList().insert(newBB->getFirstInsertionPt(), cloneLPad);
	}
	// update the edgeBBMap
	assert(predEdges.size() == newBBs.size());
	for (unsigned int i=0; i < predEdges.size(); ++i) {
		edgeBBMap[predEdges[i]] = newBBs[i];
	}

	// update PHINode of OrigBB
	for (llvm::BasicBlock::iterator inst = OrigBB->begin(), end = OrigBB->end(); inst != end; ++inst) {
		if (llvm::PHINode* phiNode = llvm::dyn_cast<llvm::PHINode>(inst)) {
			// if there's PHINode in OrigBB, replace the incoming block from 
			// the old predecessor to the corresponding newly-created blcok 
			for (unsigned int i=0; i<predBBs.size(); ++i) {
				int index = phiNode->getBasicBlockIndex(predBBs[i]);
				if (index != -1) {
					phiNode->setIncomingBlock(index, newBBs[i]);
				}
			}
		}
	}
	llvm::PHINode* pNode = llvm::PHINode::Create(lPad->getType(), newBBs.size(), "lpad.phi", lPad);
	for (llvm::SmallVectorImpl<llvm::BasicBlock*>::iterator i = newBBs.begin(),
			e = newBBs.end(); i != e; ++i) {
		pNode->addIncoming((*i)->getLandingPadInst(), *i);
	}
	// replace all uses of old landingpad to the phi node
	lPad->replaceAllUsesWith(pNode);
	// remove the old landingpad from OrigBB
	lPad->eraseFromParent();
}

void BLDag::splitCriticalLandingPads() {
	llvm::SmallVector<BLNode*, 8> lPads2Split;
	for(BLNodeIterator nodeIt = _nodes.begin(), end = _nodes.end();
			nodeIt != end; nodeIt++) {
		llvm::BasicBlock* blk = (*nodeIt)->getBlock();
		if (blk && blk->isLandingPad() && !blk->getUniquePredecessor()) {
			// this basic block is a landing pad and has mutiple predecessors
			// which makes the edges to it critical edges
			lPads2Split.push_back(*nodeIt);
		}
	}
	llvm::DenseMap<BLEdge*, llvm::BasicBlock*> edgeBBMap;
	for (llvm::SmallVector<BLNode*, 8>::iterator i = lPads2Split.begin(),
			e = lPads2Split.end(); i != e; ++i) {
		// split the node
		splitLandingPadForCriticalEdge(*i, edgeBBMap);
	}
	// update the DAG after spliting
	for (llvm::DenseMap<BLEdge*, llvm::BasicBlock*>::iterator i = edgeBBMap.begin(),
			e = edgeBBMap.end(); i != e; ++i) {
		splitUpdate(i->first, i->second);
	}
}
// Calculates a spanning tree of the DAG ignoring cycles.  Whichever
// edges are in the spanning tree will not be instrumented, but this
// implementation does not try to minimize the instrumentation overhead
// by trying to find hot edges.
void BLDag::calculateSpanningTree() {
  std::stack<BLNode*> dfsStack;

  for(BLNodeIterator nodeIt = _nodes.begin(), end = _nodes.end();
      nodeIt != end; nodeIt++) {
    (*nodeIt)->setColor(BLNode::WHITE);
  }

  getRoot()->setColor(BLNode::GRAY);
  dfsStack.push(getRoot());
  while(dfsStack.size() > 0) {
    BLNode* node = dfsStack.top();

    if(node->getColor() == BLNode::BLACK) {
		dfsStack.pop();
		continue;
	}
    BLNode* nextNode;
    bool forward = true;
    BLEdgeIterator succEnd = node->succEnd();
//
//    node->setColor(BLNode::WHITE);
    // first iterate over successors then predecessors
    for(BLEdgeIterator edge = node->succBegin(), predEnd = node->predEnd();
        edge != predEnd; edge++) {
      if(edge == succEnd) {
        edge = node->predBegin();
        forward = false;
      }

      // Ignore split edges
      if ((*edge)->getType() == BLEdge::SPLITEDGE
			  || (*edge)->getType() == BLEdge::BACKEDGE
			  || (*edge)->getType() == BLEdge::CALLEDGE)
        continue;

      nextNode = forward? (*edge)->getTarget(): (*edge)->getSource();
//      if(nextNode->getColor() != BLNode::WHITE) {
//        nextNode->setColor(BLNode::WHITE);
//        makeEdgeSpanning((BLEdge*)(*edge));
//      }
	  if (nextNode->getColor() == BLNode::WHITE) {
		  makeEdgeSpanning((BLEdge*)(*edge));
		  nextNode->setColor(BLNode::GRAY);
		  dfsStack.push(nextNode);
	  } 
    }
	node->setColor(BLNode::BLACK);
  }

  for(BLEdgeIterator edge = _edges.begin(), end = _edges.end();
      edge != end; edge++) {
    BLEdge* instEdge = (BLEdge*) (*edge);
      // safe since createEdge is overriden
    if(!instEdge->isInSpanningTree()
		&& (*edge)->getType() != BLEdge::SPLITEDGE
		&& (*edge)->getType() != BLEdge::BACKEDGE
		&& (*edge)->getType() != BLEdge::CALLEDGE) {
      _chordEdges.push_back(instEdge);
	  instEdge->setIsChordEdge(true);
	}
  }
}

// Pushes initialization further down in order to group the first
// increment and initialization.
void BLDag::pushInitialization() {
//  BLEdge* exitRootEdge =
//                (BLEdge*) getExitRootEdge();
//  exitRootEdge->setIsInitialization(true);
//  pushInitializationFromEdge(exitRootEdge);
  std::stack<BLNode*> nodeStack;
  nodeStack.push((BLNode*)getRoot());
  while (nodeStack.size() > 0) {
	  BLNode* node = nodeStack.top();
	  nodeStack.pop();
	  for (BLEdgeIterator succ = node->succBegin(), end = node->succEnd();
			  succ != end; ++succ) {
		  BLEdge* edge = (BLEdge*)(*succ);
		  if(edge->getType() != BLEdge::SPLITEDGE
				  && edge->getType() != BLEdge::BACKEDGE
				  && edge->getType() != BLEdge::CALLEDGE) {
			  if (edge->isChordEdge()) {
				  edge->setIsInitialization(true);
			  } else {
				  BLNode* targetNode = (BLNode*)(edge->getTarget());
				  if (targetNode->getPredDagEdgeNum() == 1) {
					  nodeStack.push(targetNode);
				  } else {
					  edge->setIsInitialization(true);
				  }
			  }
		  }
	  }
  }
}

void BLDag::labelBLPathEnd() {
	  BLNode* node = (BLNode*)getExit();
	  for (BLEdgeIterator pred = node->predBegin(), end = node->predEnd();
			  pred != end; ++pred) {
		  BLEdge* edge = (BLEdge*)(*pred);
		  if(edge->getType() != BLEdge::SPLITEDGE
				  && edge->getType() != BLEdge::BACKEDGE
				  && edge->getType() != BLEdge::CALLEDGE) {
				  edge->setIsBLPathEnd(true);
		  }
	  }
}

// Removes phony edges from the successor list of the source, and the
// predecessor list of the target.
void BLDag::unlinkPhony() {
  BLEdge* edge;

  for(BLEdgeIterator next = _edges.begin(),
      end = _edges.end(); next != end; next++) {
    edge = (*next);

	if (edge->getType() == BLEdge::PHONYEDGE) {
      unlinkEdge(edge);
    }
  }
}

// Generate a .dot graph to represent the DAG and pathNumbers
void BLDag::generateDotGraph(bool isTraceRecovery) {
  std::string errorInfo;
  std::string functionName = getFunction()->getName().str();
  std::string filename = "./dot/pathdag." + functionName + ".dot";

  DEBUG (llvm::dbgs() << "Writing '" << filename << "'...\n");
  llvm::raw_fd_ostream dotFile(filename.c_str(), errorInfo);

  if (!errorInfo.empty()) {
    llvm::errs() << "Error opening '" << filename.c_str() <<"' for writing!";
    llvm::errs() << "\n";
    return;
  }

  dotFile << "digraph " << functionName << " {\n";

  for( BLEdgeIterator edge = _edges.begin(), end = _edges.end();
       edge != end; edge++) {
//	  if ((*edge)->getType() == BLEdge::PHONYEDGE)
//		  continue;
    std::string sourceName = (*edge)->getSource()->getName();
    std::string targetName = (*edge)->getTarget()->getName();

    dotFile << "\t\"" << sourceName.c_str() << "\" -> \""
            << targetName.c_str() << "\" ";
	int64_t inc;
	if (isTraceRecovery) {
		inc = (*edge)->getWeight();
	} else {
		inc = ((BLEdge*)(*edge))->getIncrement();
		if (((BLEdge*)(*edge))->isInSpanningTree()) {
			dotFile <<"[penwidth=4]";
		}
		if (((BLEdge*)(*edge))->isInitialization()) {
			dotFile <<"[style=dotted]";
		}
		if (((BLEdge*)(*edge))->isBLPathEnd()) {
			dotFile <<"[arrowhead=box]";
		}
	}
    switch( (*edge)->getType() ) {
    case BLEdge::NORMAL:
      dotFile << "[label=" << inc << "] [color=black];\n";
      break;

    case BLEdge::BACKEDGE:
      dotFile << "[color=cyan];\n";
      break;

    case BLEdge::PHONYEDGE:
      dotFile << "[label=" << inc
              << "] [color=blue];\n";
      break;

    case BLEdge::SPLITEDGE:
      dotFile << "[color=violet];\n";
      break;

//    case BLEdge::SPLITEDGE_PHONY:
//      dotFile << "[label=" << inc << "] [color=red];\n";
//      break;

    case BLEdge::CALLEDGE:
      dotFile << "[label=" << inc << "] [color=green];\n";
      break;
    }
  }

  dotFile << "}\n";
}

// Removes the edge from the appropriate predecessor and successor
// lists.
void BLDag::unlinkEdge(BLEdge* edge) {
  if(edge == getExitRootEdge())
    DEBUG(llvm::dbgs() << " Removing exit->root edge\n");

  edge->getSource()->removeSuccEdge(edge);
  edge->getTarget()->removePredEdge(edge);
}

// Makes an edge part of the spanning tree.
void BLDag::makeEdgeSpanning(BLEdge* edge) {
  edge->setIsInSpanningTree(true);
  _treeEdges.push_back(edge);
}

// Depth first algorithm for determining the chord increments.
void BLDag::calculateChordIncrementsDfs(long weight, const BLNode* v, const BLEdge* e) {
  BLEdge* f;

  for(BLEdgeIterator treeEdge = _treeEdges.begin(),
        end = _treeEdges.end(); treeEdge != end; treeEdge++) {
    f = (BLEdge*) *treeEdge;
    if(e != f && v == f->getTarget()) {
      calculateChordIncrementsDfs(
        calculateChordIncrementsDir(e,f)*(weight) +
        f->getWeight(), f->getSource(), f);
    }
    if(e != f && v == f->getSource()) {
      calculateChordIncrementsDfs(
        calculateChordIncrementsDir(e,f)*(weight) +
        f->getWeight(), f->getTarget(), f);
    }
  }

  for(BLEdgeIterator chordEdge = _chordEdges.begin(),
        end = _chordEdges.end(); chordEdge != end; chordEdge++) {
    f = *chordEdge;
    if(v == f->getSource() || v == f->getTarget()) {
      f->setIncrement(f->getIncrement() +
                      calculateChordIncrementsDir(e,f)*weight);
    }
  }
}

// Determines the relative direction of two edges.
int BLDag::calculateChordIncrementsDir(const BLEdge* e, const BLEdge* f) {
  if( e == NULL)
    return(1);
  else if(e->getSource() == f->getTarget()
          || e->getTarget() == f->getSource())
    return(1);

  return(-1);
}

void BLDag::getStatistics() {
	llvm::errs() << "Function: " << getFunction()->getName().str() <<'\n';
	llvm::errs() << "Num of nodes: " << _nodes.size() << '\n';
	llvm::errs() << "Num of edges: " << _edges.size() << '\n';
	llvm::errs() << "Num of chord edges: " << _chordEdges.size() << '\n';
	int backEdgeNum = 0;
	int normalEdgeNum = 0;
	int splitEdgeNum = 0;
	int callEdgeNum = 0;
	int phonyEdgeNum = 0;
	for (BLEdgeIterator edge = _edges.begin(), end = _edges.end(); edge != end; ++edge) {
		switch((*edge)->getType()) {
			case BLEdge::NORMAL:
				++normalEdgeNum;
				break;
			case BLEdge::BACKEDGE:
				++backEdgeNum;
				break;
			case BLEdge::PHONYEDGE:
				++phonyEdgeNum;
				break;
			case BLEdge::SPLITEDGE:
				++splitEdgeNum;
				break;
			case BLEdge::CALLEDGE:
				++callEdgeNum;
				break;
		}
	}
	llvm::errs() << "Num of normal edges: " << normalEdgeNum <<'\n';
	llvm::errs() << "Num of back edges: " << backEdgeNum <<'\n';
	llvm::errs() << "Num of split edges: " << splitEdgeNum <<'\n';
	llvm::errs() << "Num of call edges: " << callEdgeNum <<'\n';
	llvm::errs() << "Num of phony edges: " << phonyEdgeNum <<'\n';
	llvm::errs() << "Num of BL paths: " << getRoot()->getNumberPaths() <<'\n';
}

void BLDag::outputPathNumbers() {
	  for(BLNodeIterator node = _nodes.begin(), end = _nodes.end(); node != end;
			  ++node) {
		  int numOfPaths = (*node)->getNumberPaths();
		  llvm::errs() << "Num of Paths: " << numOfPaths << '\n';
	  }
	  llvm::errs() << "Num of tree edges: " << _treeEdges.size() << '\n';
	  llvm::errs() << "Num of chord edges: " << _chordEdges.size() << '\n';
	  llvm::errs() << "Num of edges: " << _edges.size() << '\n';
	  llvm::errs() << "Num of nodes: " << _nodes.size() << '\n';
	  int numOfInitEdges = 0;
	  int numOfBLPathEndingEdges = 0;
	  for (BLEdgeIterator edge = _edges.begin(), end = _edges.end(); edge != end; ++edge) {
		 if (((BLEdge*)(*edge))->isInitialization()) {
			 numOfInitEdges++;
		 }
		 if (((BLEdge*)(*edge))->isBLPathEnd()) {
			 numOfBLPathEndingEdges++;
		 }
	  }
	  llvm::errs() << "Num of inti edges: " << numOfInitEdges << '\n';
	  llvm::errs() << "Num of counter increment edges: " << numOfBLPathEndingEdges << '\n';
}

const BLNodeVector& BLDag::getDagNodes() const{
	return _nodes;
}

BLNodeVector& BLDag::getDagNodes() {
	return _nodes;
}

// BLDag << operator overloading
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const BLEdge& edge) {
	os << "[" << edge.getSource()->getName() << " -> "
		<< edge.getTarget()->getName() << "] init: "
		<< (edge.isInitialization() ? "yes" : "no")
		<< " incr:" << edge.getIncrement() << " blend: "
		<< (edge.isBLPathEnd() ? "yes" : "no");
	return(os);
}

