/*
 * =====================================================================================
 *
 *       Filename:  BLWPPRecoveryEdge.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Sunday, December 28, 2014 09:44:42 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chunbai Yang (CBY), chunbyang2@gapps.cityu.edu.hk
 *        Company:  City University of Hong Kong
 *
 * =====================================================================================
 */

#include "../include/BLWPPRecoveryEdge.h"
using namespace pathTracing;

unsigned BLWPPRecoveryEdge::getBLWeight() const {
	return blWeight;
}

void BLWPPRecoveryEdge::setBLWeight(unsigned weight) {
	blWeight = weight;
}

BLEdge* BLWPPRecoveryEdge::getCallEdge() {
	return callEdge;
}

const BLEdge* BLWPPRecoveryEdge::getCallEdge() const {
	return callEdge;
}

void BLWPPRecoveryEdge::setCallEdge(BLEdge* e) {
	callEdge = e;
}
