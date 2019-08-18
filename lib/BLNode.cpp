/*
 * =====================================================================================
 *
 *       Filename:  BLNode.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Monday, May 19, 2014 11:02:47 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  
 *        Company: 
 *
 * =====================================================================================
 */

#include "../include/BLNode.h"
#include "llvm/IR/BasicBlock.h" //llvm::BasicBlock
#include "llvm/Support/Debug.h"
#include <sstream>

using namespace pathTracing;

// Creates a new BLNode from a BasicBlock.
BLNode::BLNode(llvm::BasicBlock* BB) :
	_basicBlock(BB), _numberPaths(0), _color(WHITE), _startingPathNumber(NULL), _endingPathNumber(NULL), _pathPHI(NULL) 
{
	static unsigned nextUID = 0;
	_uid = nextUID++;
}

// Returns the basic block for the BLNode
llvm::BasicBlock* BLNode::getBlock() {
  return(_basicBlock);
}

const llvm::BasicBlock* BLNode::getBlock() const{
  return(_basicBlock);
}

unsigned BLNode::getUID() const{
  return(_uid);
}

// Returns the number of paths to the exit starting at the node.
unsigned BLNode::getNumberPaths() const{
  return(_numberPaths);
}

// Sets the number of paths to the exit starting at the node.
void BLNode::setNumberPaths(unsigned numberPaths) {
  _numberPaths = numberPaths;
}

// Gets the NodeColor used in graph algorithms.
BLNode::NodeColor BLNode::getColor() const{
  return(_color);
}

// Sets the NodeColor used in graph algorithms.
void BLNode::setColor(BLNode::NodeColor color) {
  _color = color;
}

// Returns an iterator over predecessor edges. Includes phony and
// backedges.
BLEdgeIterator BLNode::predBegin() {
  return(_predEdges.begin());
}

BLEdgeConstIterator BLNode::predBegin() const{
  return(_predEdges.cbegin());
}

// Returns the end sentinel for the predecessor iterator.
BLEdgeIterator BLNode::predEnd() {
  return(_predEdges.end());
}

BLEdgeConstIterator BLNode::predEnd() const{
  return(_predEdges.cend());
}

// Returns the number of predecessor edges.  Includes phony and
// backedges.
unsigned BLNode::getNumberPredEdges() const{
  return(_predEdges.size());
}

// Returns an iterator over successor edges. Includes phony and
// backedges.
BLEdgeIterator BLNode::succBegin() {
  return(_succEdges.begin());
}

BLEdgeConstIterator BLNode::succBegin() const{
  return(_succEdges.cbegin());
}

// Returns the end sentinel for the successor iterator.
BLEdgeIterator BLNode::succEnd() {
  return(_succEdges.end());
}

BLEdgeConstIterator BLNode::succEnd() const{
  return(_succEdges.cend());
}

// Returns the number of successor edges.  Includes phony and
// backedges.
unsigned BLNode::getNumberSuccEdges() const{
  return(_succEdges.size());
}

// Add an edge to the predecessor list.
void BLNode::addPredEdge(BLEdge* edge) {
  _predEdges.push_back(edge);
}

// Remove an edge from the predecessor list.
void BLNode::removePredEdge(const BLEdge* edge) {
  removeEdge(_predEdges, edge);
}

// Add an edge to the successor list.
void BLNode::addSuccEdge(BLEdge* edge) {
  _succEdges.push_back(edge);
}

// Remove an edge from the successor list.
void BLNode::removeSuccEdge(const BLEdge* edge) {
  removeEdge(_succEdges, edge);
}

// Returns the name of the BasicBlock being represented.  If BasicBlock
// is null then returns "<null>".  If BasicBlock has no name, then
// "<unnamed>" is returned.  Intended for use with debug output.
std::string BLNode::getName() const{
  std::stringstream name;

  if(getBlock() != NULL) {
    if(getBlock()->hasName()) {
      std::string tempName(getBlock()->getName());
      name << tempName.c_str() << " (" << _uid << ")";
    } else
      name << "<unnamed> (" << _uid << ")";
  } else
    name << "<null> (" << _uid << ")";

  return name.str();
}

// Removes an edge from an edgeVector.  Used by removePredEdge and
// removeSuccEdge.
void BLNode::removeEdge(BLEdgeVector& v, const BLEdge* e) {
  // TODO: Avoid linear scan by using a set instead
  for(BLEdgeIterator i = v.begin(),
        end = v.end();
      i != end;
      ++i) {
    if((*i) == e) {
      v.erase(i);
      break;
    }
  }
}

int BLNode::getPredDagEdgeNum() const{
	int num = 0;
	for (BLEdgeConstIterator pred = this->predBegin(), end = this->predEnd();
			pred != end; ++pred) {
		BLEdge* edge = (BLEdge*)(*pred);
		if (edge->getType() == BLEdge::NORMAL
				|| edge->getType() == BLEdge::PHONYEDGE) {
			++num;
		}
	}
	return num;
}

int BLNode::getSuccDagEdgeNum() const{
	int num = 0;
	for (BLEdgeConstIterator succ = this->succBegin(), end = this->succEnd();
			succ != end; ++succ) {
		BLEdge* edge = (BLEdge*)(*succ);
		if (edge->getType() == BLEdge::NORMAL
				|| edge->getType() == BLEdge::PHONYEDGE) {
			++num;
		}
	}
	return num;
}

// Sets the Value corresponding to the pathNumber register, constant,
// or phinode.  Used by the instrumentation code to remember path
// number Values.
llvm::Value* BLNode::getStartingPathNumber(){
  return(_startingPathNumber);
}

// Sets the Value of the pathNumber.  Used by the instrumentation code.
void BLNode::setStartingPathNumber(llvm::Value* pathNumber) {
  DEBUG(llvm::dbgs() << "  SPN-" << getName() << " <-- " << (pathNumber ?
                                                       pathNumber->getName() :
                                                       "unused") << "\n");
  _startingPathNumber = pathNumber;
}

llvm::Value* BLNode::getEndingPathNumber(){
  return(_endingPathNumber);
}

void BLNode::setEndingPathNumber(llvm::Value* pathNumber) {
  DEBUG(llvm::dbgs() << "  EPN-" << getName() << " <-- "
               << (pathNumber ? pathNumber->getName() : "unused") << "\n");
  _endingPathNumber = pathNumber;
}

// Get the PHINode Instruction for this node.  Used by instrumentation
// code.
llvm::PHINode* BLNode::getPathPHI() {
  return(_pathPHI);
}

// Set the PHINode Instruction for this node.  Used by instrumentation
// code.
void BLNode::setPathPHI(llvm::PHINode* pathPHI) {
  _pathPHI = pathPHI;
}
