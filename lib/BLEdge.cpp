/*
 * =====================================================================================
 *
 *       Filename:  BLEdge.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Monday, May 19, 2014 11:11:36 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  
 *        Company: 
 *
 * =====================================================================================
 */

#include "../include/BLEdge.h"

using namespace pathTracing;
// Constructor for BLEdge.

BLEdge::BLEdge(BLNode* source, BLNode* target)
	: _source(source), _target(target), _weight(0), _edgeType(BLEdge::NORMAL), 
	_phonyRoot(NULL), _phonyExit(NULL), _increment(0), _isInSpanningTree(false),
	_isChord(false), _isInitialization(false), _isBLPathEnd(false), _hasInstrumentation(false) {}

// Returns the source node of this edge.
BLNode* BLEdge::getSource() {
  return(_source);
}

const BLNode* BLEdge::getSource() const {
  return(_source);
}

// Returns the target node of this edge.
BLNode* BLEdge::getTarget() {
  return(_target);
}

const BLNode* BLEdge::getTarget() const {
  return(_target);
}

// Sets the type of the edge.
BLEdge::EdgeType BLEdge::getType() const {
  return _edgeType;
}

// Gets the type of the edge.
void BLEdge::setType(BLEdge::EdgeType type) {
  _edgeType = type;
}

// Returns the weight of this edge.  Used to decode path numbers to sequences
// of basic blocks.
unsigned BLEdge::getWeight() const{
  return(_weight);
}

// Sets the weight of the edge.  Used during path numbering.
void BLEdge::setWeight(unsigned weight) {
  _weight = weight;
}

// Gets the phony edge originating at the root.
BLEdge* BLEdge::getPhonyRoot() {
  return _phonyRoot;
}

const BLEdge* BLEdge::getPhonyRoot() const{
  return _phonyRoot;
}

// Sets the phony edge originating at the root.
void BLEdge::setPhonyRoot(BLEdge* phonyRoot) {
  _phonyRoot = phonyRoot;
}

// Gets the phony edge terminating at the exit.
BLEdge* BLEdge::getPhonyExit() {
  return _phonyExit;
}

const BLEdge* BLEdge::getPhonyExit() const{
  return _phonyExit;
}

// Sets the phony edge terminating at the exit.
void BLEdge::setPhonyExit(BLEdge* phonyExit) {
  _phonyExit = phonyExit;
}

// Sets the target node of this edge.  Required to split edges.
void BLEdge::setTarget(BLNode* node) {
  _target = node;
}

// Returns whether this edge is in the spanning tree.
bool BLEdge::isInSpanningTree() const {
  return(_isInSpanningTree);
}

// Sets whether this edge is in the spanning tree.
void BLEdge::setIsInSpanningTree(bool isInSpanningTree) {
  _isInSpanningTree = isInSpanningTree;
}

// Returns whether this edge will be instrumented with a path number
// initialization.
bool BLEdge::isInitialization() const {
  return(_isInitialization);
}

// Sets whether this edge will be instrumented with a path number
// initialization.
void BLEdge::setIsInitialization(bool isInitialization) {
  _isInitialization = isInitialization;
}

// Returns whether this edge will be instrumented with a path counter
// increment.  Notice this is incrementing the path counter
// corresponding to the path number register.  The path number
// increment is determined by getIncrement().
bool BLEdge::isBLPathEnd() const {
  return(_isBLPathEnd);
}

// Sets whether this edge will be instrumented with a path counter
// increment.
void BLEdge::setIsBLPathEnd(bool isBLPathEnd) {
  _isBLPathEnd = isBLPathEnd;
}

bool BLEdge::isChordEdge() const {
	return (_isChord);
}

void BLEdge::setIsChordEdge(bool isChordEdge) {
	_isChord = isChordEdge;
}

// Gets the path number increment that this edge will be instrumented
// with. The path number keeps track of which path number
// the program is on.
int64_t BLEdge::getIncrement() const {
  return(_increment);
}

// Set whether this edge will be instrumented with a path number
// increment.
void BLEdge::setIncrement(int64_t increment) {
  _increment = increment;
}

// True iff the edge has already been instrumented.
bool BLEdge::hasInstrumentation() const{
  return(_hasInstrumentation);
}

// Set whether this edge has been instrumented.
void BLEdge::setHasInstrumentation(bool hasInstrumentation) {
  _hasInstrumentation = hasInstrumentation;
}

// Returns the successor number of this edge in the source.
unsigned BLEdge::getSuccessorNumber() const{
  const BLNode* sourceNode = getSource();
  const BLNode* targetNode = getTarget();
  const llvm::BasicBlock* source = sourceNode->getBlock();
  const llvm::BasicBlock* target = targetNode->getBlock();

  if(source == NULL || target == NULL)
    return(0);

  const llvm::TerminatorInst* terminator = source->getTerminator();

  unsigned i;
  for(i=0; i < terminator->getNumSuccessors(); ++i) {
    if(terminator->getSuccessor(i) == target)
      break;
  }
  return(i);
}
