/*
 * =====================================================================================
 *
 *       Filename:  BLWPPRecoveryDag.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Monday, December 29, 2014 10:03:48 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chunbai Yang (CBY), chunbyang2@gapps.cityu.edu.hk
 *        Company:  City University of Hong Kong
 *
 * =====================================================================================
 */

#include "../include/BLWPPRecoveryDag.h"
#include <sstream>

using namespace pathTracing;

void BLWPPRecoveryDag::getUnfinishedPath(std::vector<uint32_t>& paths, uint32_t& pathState, uint32_t& uid) {
	// walk through the WPP Dag by each wpp path id
	// sum up the weight of its corresponding BL Dag path
	uint32_t originalPathID = 0;
	const BLWPPRecoveryEdge* lastVisitedCallEdge = NULL;
	const BLEdge* edgeInPath = NULL;
	for (unsigned i=0; i<paths.size(); ++i) {
		unsigned rValue = paths[i];
		const BLNode* currentProcessingNode = this->getRoot();
		while (currentProcessingNode != this->getExit()) {
			unsigned maxWeight = 0;
			edgeInPath = NULL;
			for( BLEdgeConstIterator succ = currentProcessingNode->succBegin(), end = currentProcessingNode->succEnd();
					succ != end; succ++ ) {
				if( (*succ)->getType() == BLEdge::BACKEDGE ||
						(*succ)->getType() == BLEdge::SPLITEDGE ||
						(*succ)->getType() == BLEdge::CALLEDGE )
					continue;
				unsigned edgeWeight = (*succ)->getWeight();
				if (edgeWeight <= rValue && edgeWeight >= maxWeight) {
					maxWeight = edgeWeight;
					edgeInPath = (*succ);
				}
			}
			if (!edgeInPath) {
				std::stringstream ss;
				ss<<"The path "<< paths[i] << " of function "<<this->getFunction()->getName().str()<<" is not found!\n";
				assert(ss.str().c_str());	
			}
			rValue -= edgeInPath->getWeight();
			currentProcessingNode = edgeInPath->getTarget();
			if (currentProcessingNode == getExit() && rValue != 0) {
				std::stringstream ss;
				ss<<"The path "<< paths[i] << " of function "<<this->getFunction()->getName().str()<<" is not found!\n";
				assert(ss.str().c_str());	
			}
			BLWPPRecoveryEdge* e = (BLWPPRecoveryEdge*)edgeInPath;
			if (e->getCallEdge() == NULL) {
				// only phony edges originated from call edges have non-NULL call edge
				originalPathID += e->getBLWeight();
			} else if (e->getTarget() == getExit()) {
				lastVisitedCallEdge = (BLWPPRecoveryEdge*)(e->getCallEdge());
				// for phony edges originated from call edges which lead to the exit
				originalPathID += lastVisitedCallEdge->getBLWeight();
			} else {
				// for phony edges originated from call edges which starts from entry
				if (lastVisitedCallEdge != (BLWPPRecoveryEdge*)(e->getCallEdge())) {
					assert("original bl path not matched\n");
				}
			}
		}
	}
	BLWPPRecoveryEdge* lastEdgeInPath = (BLWPPRecoveryEdge*)edgeInPath;
	// the last edgeInPath processed must be a phony call edge
	assert( lastEdgeInPath->getCallEdge() != NULL && lastEdgeInPath->getTarget() == getExit() );
	pathState = originalPathID;
	uid = lastEdgeInPath->getCallEdge()->getTarget()->getUID();
}

uint32_t BLWPPRecoveryDag::getOriginalPathState(const uint32_t& wppPath) {
	uint32_t rValue = wppPath;
	unsigned originalPathState = 0;
	const BLNode* currentProcessingNode = this->getRoot();
	while (currentProcessingNode != this->getExit()) {
		unsigned maxWeight = 0;
		const BLEdge* edgeInPath = NULL;
		for( BLEdgeConstIterator succ = currentProcessingNode->succBegin(), end = currentProcessingNode->succEnd();
				succ != end; succ++ ) {
			if( (*succ)->getType() == BLEdge::BACKEDGE ||
					(*succ)->getType() == BLEdge::SPLITEDGE ||
					(*succ)->getType() == BLEdge::CALLEDGE )
				continue;
			unsigned edgeWeight = (*succ)->getWeight();
			if (edgeWeight <= rValue && edgeWeight >= maxWeight) {
				maxWeight = edgeWeight;
				edgeInPath = (*succ);
			}
		}
		if (!edgeInPath) {
			std::stringstream ss;
			ss<<"The path "<< wppPath << " of function "<<this->getFunction()->getName().str()<<" is not found!\n";
			assert(ss.str().c_str());	
		}
		rValue -= edgeInPath->getWeight();
		currentProcessingNode = edgeInPath->getTarget();
		if (currentProcessingNode == getExit() && rValue != 0) {
			std::stringstream ss;
			ss<<"The path "<< wppPath << " of function "<<this->getFunction()->getName().str()<<" is not found!\n";
			assert(ss.str().c_str());	
		}
		BLWPPRecoveryEdge* e = (BLWPPRecoveryEdge*)edgeInPath;
		if (e->getType() != BLEdge::PHONYEDGE || e->getCallEdge() == NULL) {
			// for edges that are not phony or not originated from call edges
			originalPathState += e->getBLWeight();
		} else if (e->getTarget() == getExit()) {
			// for phony edges originated from call edges which lead to the exit
			originalPathState += ((BLWPPRecoveryEdge*)(e->getCallEdge()))->getBLWeight();
		}
	}
	return originalPathState;
}

uint32_t BLWPPRecoveryDag::getOriginalBLPathID(const std::vector<uint32_t>& paths) {
	// walk through the WPP Dag by each wpp path id
	// sum up the weight of its corresponding BL Dag path
	uint32_t originalPathID = 0;
	BLWPPRecoveryEdge* lastVisitedCallEdge = NULL;
	for (unsigned i=0; i<paths.size(); ++i) {
		unsigned rValue = paths[i];
		const BLNode* currentProcessingNode = this->getRoot();
		while (currentProcessingNode != this->getExit()) {
			unsigned maxWeight = 0;
			const BLEdge* edgeInPath = NULL;
			for( BLEdgeConstIterator succ = currentProcessingNode->succBegin(), end = currentProcessingNode->succEnd();
					succ != end; succ++ ) {
				if( (*succ)->getType() == BLEdge::BACKEDGE ||
						(*succ)->getType() == BLEdge::SPLITEDGE ||
						(*succ)->getType() == BLEdge::CALLEDGE )
					continue;
				unsigned edgeWeight = (*succ)->getWeight();
				if (edgeWeight <= rValue && edgeWeight >= maxWeight) {
					maxWeight = edgeWeight;
					edgeInPath = (*succ);
				}
			}
			if (!edgeInPath) {
				std::stringstream ss;
				ss<<"The path "<< paths[i] << " of function "<<this->getFunction()->getName().str()<<" is not found!\n";
				assert(ss.str().c_str());	
			}
			rValue -= edgeInPath->getWeight();
			currentProcessingNode = edgeInPath->getTarget();
			if (currentProcessingNode == getExit() && rValue != 0) {
				std::stringstream ss;
				ss<<"The path "<< paths[i] << " of function "<<this->getFunction()->getName().str()<<" is not found!\n";
				assert(ss.str().c_str());	
			}
			BLWPPRecoveryEdge* e = (BLWPPRecoveryEdge*)edgeInPath;
			if (e->getType() != BLEdge::PHONYEDGE || e->getCallEdge() == NULL) {
				// for edges that are not phony or not originated from call edges
				originalPathID += e->getBLWeight();
			} else if (e->getTarget() == getExit()) {
				lastVisitedCallEdge = (BLWPPRecoveryEdge*)(e->getCallEdge());
				// for phony edges originated from call edges which lead to the exit
				originalPathID += lastVisitedCallEdge->getBLWeight();
				if (i == paths.size()-1) {
					assert("original bl path not matched\n");
				}
			} else {
				// for phony edges originated from call edges which starts from entry
				if (lastVisitedCallEdge != (BLWPPRecoveryEdge*)(e->getCallEdge())) {
					assert("original bl path not matched\n");
				}
			}
		}
	}
	return originalPathID;
}

void BLWPPRecoveryDag::calculateOriginalBLPathNumbers() {
  BLWPPRecoveryNode* node;
  std::queue<BLNode*> bfsQueue;
  bfsQueue.push(getExit());

  while(bfsQueue.size() > 0) {
    node = (BLWPPRecoveryNode*)bfsQueue.front();
    bfsQueue.pop();
    unsigned prevPathNumber = node->getNumOfOriginalPaths();

	if(node == getExit())
		// The Exit node must be base case
		node->setNumOfOriginalPaths(1);
	else {
		unsigned sumPaths = 0;
		BLWPPRecoveryNode* succNode;

		for(BLEdgeIterator succ = node->succBegin(), end = node->succEnd();
				succ != end; succ++) {
			if( (*succ)->getType() == BLEdge::BACKEDGE ||
					(*succ)->getType() == BLEdge::SPLITEDGE )
				continue;

			// phony edges of call edges are ignored
			if ((*succ)->getType() == BLEdge::PHONYEDGE && ((BLWPPRecoveryEdge*)(*succ))->getCallEdge() != NULL)
				continue;

			((BLWPPRecoveryEdge*)(*succ))->setBLWeight(sumPaths);
			succNode = (BLWPPRecoveryNode*)((*succ)->getTarget());

			if( !succNode->getNumOfOriginalPaths() )
				return;
			sumPaths += succNode->getNumOfOriginalPaths();
		}

		node->setNumOfOriginalPaths(sumPaths);
	}

    if(prevPathNumber == 0 && node->getNumOfOriginalPaths() != 0) {
      for(BLEdgeIterator pred = node->predBegin(), end = node->predEnd();
          pred != end; pred++) {
        if( (*pred)->getType() == BLEdge::BACKEDGE ||
            (*pred)->getType() == BLEdge::SPLITEDGE )
			continue;

		// phony edges of call edges are ignored
		if ((*pred)->getType() == BLEdge::PHONYEDGE && ((BLWPPRecoveryEdge*)(*pred))->getCallEdge() != NULL)
			continue;

        BLWPPRecoveryNode* nextNode = (BLWPPRecoveryNode*)((*pred)->getSource());
        // not yet visited?
        if(nextNode->getNumOfOriginalPaths() == 0)
          bfsQueue.push(nextNode);
      }
    }
  }
}

// label the call edge from its target BLNode, add phony edges for the call edge
void BLWPPRecoveryDag::splitCallNode(BLNode* node) {
	assert(node->getNumberPredEdges() == 1 && "Call node should have only one predecessor");
	BLEdge* callEdge = *(node->predBegin());

	assert(callEdge->getType() == BLEdge::NORMAL);
	assert(callEdge->getSource() != getRoot());
	assert(callEdge->getSource()->getNumberSuccEdges() == 1);

	BLWPPRecoveryEdge* rootEdge = (BLWPPRecoveryEdge*)addPhonyEdge(getRoot(), node);
	BLWPPRecoveryEdge* exitEdge = (BLWPPRecoveryEdge*)addPhonyEdge(callEdge->getSource(), getExit());

	rootEdge->setCallEdge(callEdge);
	exitEdge->setCallEdge(callEdge);

	callEdge->setType(BLEdge::CALLEDGE);
	callEdge->setPhonyRoot(rootEdge);
	callEdge->setPhonyExit(exitEdge);
}
