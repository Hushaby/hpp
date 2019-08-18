/*
 * =====================================================================================
 *
 *       Filename:  BLWPPRecoveryEdge.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Sunday, December 28, 2014 09:38:44 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chunbai Yang (CBY), chunbyang2@gapps.cityu.edu.hk
 *        Company:  City University of Hong Kong
 *
 * =====================================================================================
 */

#include "BLEdge.h"

namespace pathTracing {

class BLWPPRecoveryEdge : public BLEdge {
	public:
		BLWPPRecoveryEdge(BLNode* source, BLNode* target) : 
			BLEdge(source, target), blWeight(0), callEdge(NULL){}
		unsigned getBLWeight() const;
		void setBLWeight(unsigned weight);
		BLEdge* getCallEdge();
		const BLEdge* getCallEdge() const;
		void setCallEdge(BLEdge* e);
	private:
		unsigned blWeight;
		BLEdge* callEdge;
};
} // end of namespace pathTracing
