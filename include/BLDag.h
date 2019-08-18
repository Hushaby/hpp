//===- BLDag.h ----------------------------------------*- C++ -*---===//

#ifndef BLDAG_H
#define BLDAG_H

#include "BLNode.h"
#include "BLEdge.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalVariable.h"
#include <stack>
#include <vector>
#include "llvm/Support/raw_ostream.h"
#include <queue>

namespace pathTracing {

// Represents the Ball Larus DAG for a given Function.  Can calculate
// various properties required for instrumentation or analysis.  E.g. the
// edge weights that determine the path number.
class BLDag {
public:
  // Initializes a BLDag from the CFG of a given function.  Must
  // call init() after creation
  BLDag(llvm::Function* F);

  // Initialization that requires virtual functions which are not fully
  // functional in the constructor.
  void init();
  void initWPPDag();

  // Frees all memory associated with the DAG.
  virtual ~BLDag();

  // Calculate the path numbers by assigning edge increments as prescribed
  // in Ball-Larus path profiling.
  void calculatePathNumbers();

  // Returns the number of paths for the DAG.
  unsigned getNumberOfPaths() const;

  // Returns the root (i.e. entry) node for the DAG.
  BLNode* getRoot();
  const BLNode* getRoot() const;

  // Returns the exit node for the DAG.
  BLNode* getExit();
  const BLNode* getExit() const;

  // Returns the function for the DAG.
  llvm::Function* getFunction();
  const llvm::Function* getFunction() const;

  // Clears the node colors.
  void clearColors(BLNode::NodeColor color);
  
  int getNodesNum() const;
  int getEdgesNum() const;

  std::vector<BLNode*>* getPathNode(int pathID);
  std::vector<const llvm::BasicBlock*>* getPathBB(unsigned pathID) const;
  bool isProcExitPath(unsigned pathID) const;


  // Returns the Exit->Root edge. This edge is required for creating
  // directed cycles in the algorithm for moving instrumentation off of
  // the spanning tree
  BLEdge* getExitRootEdge();
  const BLEdge* getExitRootEdge() const;

  // Calculates the increments for the chords, thereby removing
  // instrumentation from the spanning tree edges. Implementation is based
  // on the algorithm in Figure 4 of [Ball94]
  void calculateChordIncrements();

  // Updates the state when an edge has been split
  void splitUpdate(BLEdge* formerEdge, llvm::BasicBlock* newBlock);
  // split the landing pads preceded with critical edges and update the DAG
  void splitCriticalLandingPads();

  // Calculates a spanning tree of the DAG ignoring cycles.  Whichever
  // edges are in the spanning tree will not be instrumented, but this
  // implementation does not try to minimize the instrumentation overhead
  // by trying to find hot edges.
  void calculateSpanningTree();

  // Pushes initialization further down in order to group the first
  // increment and initialization.
  void pushInitialization();

  // label the edges which end BL paths
  void labelBLPathEnd();

  // Removes phony edges from the successor list of the source, and the
  // predecessor list of the target.
  void unlinkPhony();

  // Generate dot graph for the function
  void generateDotGraph(bool isTraceRecovery);

  void outputPathNumbers();

  void getStatistics();

//  std::queue<BLNode*> getRoot2TargetPath(llvm::BasicBlock *targetBlock, int pathID);

  BLNodeVector& getDagNodes();
  const BLNodeVector& getDagNodes() const;

private:
  // All nodes in the DAG.
  BLNodeVector _nodes;

  // All edges in the DAG.
  BLEdgeVector _edges;

  // All backedges in the DAG.
  BLEdgeVector _backEdges;

  virtual BLNode* createNode(llvm::BasicBlock* BB);

  virtual BLEdge* createEdge(BLNode* source, BLNode* target);

  // Proxy to node's constructor.  Updates the DAG state.
  BLNode* addNode(llvm::BasicBlock* BB);

  // Proxy to edge's constructor.  Updates the DAG state.
  BLEdge* addEdge(BLNode* source, BLNode* target);

  // The root (i.e. entry) node for this DAG.
  BLNode* _root;

  // The exit node for this DAG.
  BLNode* _exit;

  // The function represented by this DAG.
  llvm::Function* _function;

  BLEdgeVector _treeEdges; // All edges in the spanning tree.
  BLEdgeVector _chordEdges; // All edges not in the spanning tree.

  std::queue<BLNode*> pathQueue;

  // Processes one node and its imediate edges for building the DAG.
  void buildNode(BLBlockNodeMap& inDag, std::stack<BLNode*>& dfsStack);

  // Process an edge in the CFG for DAG building.
  void buildEdge(BLBlockNodeMap& inDag, std::stack<BLNode*>& dfsStack,
                 BLNode* currentNode, llvm::BasicBlock* succBB);

  // The weight on each edge is the increment required along any path that
  // contains that edge.
  uint64_t calculatePathNumbersFrom(BLNode* node);

  // Adds a backedge with its phony edges.  Updates the DAG state.
//  void addBackedge(BLNode* source, BLNode* target);
  void processPhonyEdgesForBackEdges();
  void processPhonyEdgesForCallEdges();
  // split basic blocks at call instructions
  void splitCallBlocks();

  void splitNode(BLNode* node);
  virtual void splitCallNode(BLNode* node);

  // Removes the edge from the appropriate predecessor and successor lists.
  void unlinkEdge(BLEdge* edge);

  // Makes an edge part of the spanning tree.
  void makeEdgeSpanning(BLEdge* edge);

  // Depth first algorithm for determining the chord increments.f
  void calculateChordIncrementsDfs(long weight, const BLNode* v, const BLEdge* e);

  // Determines the relative direction of two edges.
  int calculateChordIncrementsDir(const BLEdge* e, const BLEdge* f);

//  bool saveIfInPath(BLNode* node, int rValue);
protected:
  BLEdge* addPhonyEdge(BLNode* source, BLNode* target);

};
  // BLDag << operator overloading
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const BLEdge& edge) LLVM_ATTRIBUTE_USED;

} // end of namespace pathTracing

#endif
