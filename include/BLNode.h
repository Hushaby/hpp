//===- BLNode.h ----------------------------------------*- C++ -*---===//

#ifndef BLNODE_H
#define BLNODE_H

#include "BLEdge.h"

#include "llvm/Support/DataTypes.h"
#include "llvm/IR/Value.h" //llvm::Value
#include "llvm/IR/BasicBlock.h" //llvm::BasicBlock
#include "llvm/IR/Instructions.h" //llvm::PHINode
#include <stack>
#include <vector>
#include <string>

namespace pathTracing {

class BLNode;
class BLEdge;

// typedefs for storage/ interators of various DAG components
typedef std::vector<BLNode*> BLNodeVector;
typedef std::vector<BLNode*>::iterator BLNodeIterator;
typedef std::vector<BLNode*>::const_iterator BLNodeConstIterator;
typedef std::map<llvm::BasicBlock*, BLNode*> BLBlockNodeMap;
typedef std::stack<BLNode*> BLNodeStack;
typedef std::vector<BLEdge*> BLEdgeVector;
typedef std::vector<BLEdge*>::iterator BLEdgeIterator;
typedef std::vector<BLEdge*>::const_iterator BLEdgeConstIterator;


// Represents a basic block with information necessary for the BallLarus
// algorithms.
class BLNode {
public:
  enum NodeColor { WHITE, GRAY, BLACK };

  // Constructor: Initializes a new Node for the given BasicBlock
  BLNode(llvm::BasicBlock* BB);

  // Returns the basic block for the BLNode
  llvm::BasicBlock* getBlock();
  const llvm::BasicBlock* getBlock() const;

  // Get/set the number of paths to the exit starting at the node.
  unsigned getNumberPaths() const;
  void setNumberPaths(unsigned numberPaths);

  unsigned getUID() const;

  // Get/set the NodeColor used in graph algorithms.
  NodeColor getColor() const;
  void setColor(NodeColor color);

  // Iterator information for predecessor edges. Includes phony and
  // backedges.
  BLEdgeIterator predBegin();
  BLEdgeConstIterator predBegin() const;
  BLEdgeIterator predEnd();
  BLEdgeConstIterator predEnd() const;

  unsigned getNumberPredEdges() const;

  // Iterator information for successor edges. Includes phony and
  // backedges.
  BLEdgeIterator succBegin();
  BLEdgeConstIterator succBegin() const;
  BLEdgeIterator succEnd();
  BLEdgeConstIterator succEnd() const;

  unsigned getNumberSuccEdges() const;

  // Add an edge to the predecessor list.
  void addPredEdge(BLEdge* edge);

  // Remove an edge from the predecessor list.
  void removePredEdge(const BLEdge* edge);

  // Add an edge to the successor list.
  void addSuccEdge(BLEdge* edge);

  // Remove an edge from the successor list.
  void removeSuccEdge(const BLEdge* edge);

  // Returns the name of the BasicBlock being represented.  If BasicBlock
  // is null then returns "<null>".  If BasicBlock has no name, then
  // "<unnamed>" is returned.  Intended for use with debug output.
  std::string getName() const;

  // Get/sets the Value corresponding to the pathNumber register,
  // constant or phinode.  Used by the instrumentation code to remember
  // path number Values.
  llvm::Value* getStartingPathNumber();
  void setStartingPathNumber(llvm::Value* pathNumber);

  llvm::Value* getEndingPathNumber();
  void setEndingPathNumber(llvm::Value* pathNumber);

  // Get/set the PHINode Instruction for this node.
  llvm::PHINode* getPathPHI();
  void setPathPHI(llvm::PHINode* pathPHI);

  int getPredDagEdgeNum() const;
  int getSuccDagEdgeNum() const;
private:
  // The corresponding underlying BB.
  llvm::BasicBlock* _basicBlock;

  // Holds the predecessor edges of this node.
  BLEdgeVector _predEdges;

  // Holds the successor edges of this node.
  BLEdgeVector _succEdges;

  // The number of paths from the node to the exit.
  unsigned _numberPaths;

  // 'Color' used by graph algorithms to mark the node.
  NodeColor _color;

  // Unique ID to ensure naming difference with dotgraphs
  unsigned _uid;

  llvm::Value* _startingPathNumber; // The Value for the current pathNumber.
  llvm::Value* _endingPathNumber; // The Value for the current pathNumber.
  llvm::PHINode* _pathPHI; // The PHINode for current pathNumber.

  // Removes an edge from an edgeVector.  Used by removePredEdge and
  // removeSuccEdge.
  void removeEdge(BLEdgeVector& v, const BLEdge* e);
};

} // end of pathTracing namespace
#endif
