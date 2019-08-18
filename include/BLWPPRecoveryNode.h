/*
 * =====================================================================================
 *
 *       Filename:  BLWPPRecoveryNode.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Monday, December 29, 2014 11:15:15 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chunbai Yang (CBY), chunbyang2@gapps.cityu.edu.hk
 *        Company:  City University of Hong Kong
 *
 * =====================================================================================
 */

#include "BLNode.h"

namespace pathTracing {

class BLWPPRecoveryNode : public BLNode {
	public:
		BLWPPRecoveryNode(llvm::BasicBlock* BB) : BLNode(BB), numOfOriginalPaths(0){}
		unsigned getNumOfOriginalPaths() const { return numOfOriginalPaths; }
		void setNumOfOriginalPaths(unsigned numOfPaths) { numOfOriginalPaths = numOfPaths; }
	private:
		unsigned numOfOriginalPaths;
};
} // end of namespace pathTracing
