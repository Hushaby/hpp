/*
 * =====================================================================================
 *
 *       Filename:  BLWPPRecoveryDag.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Sunday, December 28, 2014 09:28:56 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chunbai Yang (CBY), chunbyang2@gapps.cityu.edu.hk
 *        Company:  City University of Hong Kong
 *
 * =====================================================================================
 */

#include "BLDag.h"
#include "BLWPPRecoveryEdge.h"
#include "BLWPPRecoveryNode.h"
#include <limits>
#include <exception>
#include <stdio.h>

namespace pathTracing {

class BLWPPRecoveryDag : public BLDag {
	public:
//		enum PathDecodingState { MATCH, NOT_MATCH, ERROR };
		BLWPPRecoveryDag(llvm::Function* F) : BLDag(F) {}
		void calculateOriginalBLPathNumbers();
		void getUnfinishedPath(std::vector<uint32_t>& paths, uint32_t& pathState, uint32_t& uid);
		uint32_t getOriginalBLPathID(const std::vector<uint32_t>& paths);
		uint32_t getOriginalPathState(const uint32_t& wppPath);
	private:
		BLEdge* createEdge(BLNode* source, BLNode* target) {
			return( new BLWPPRecoveryEdge(source, target) );
		}
		BLNode* createNode(llvm::BasicBlock* BB) {
			return( new BLWPPRecoveryNode(BB) );
		}
		void splitCallNode(BLNode* node);
};
} // end of namespace pathTracing
