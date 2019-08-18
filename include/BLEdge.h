//===- BLEdge.h ----------------------------------------*- C++ -*---===//

#ifndef BLEDGE_H
#define BLEDGE_H

#include "BLNode.h"

#include "llvm/IR/Instructions.h"
#include <vector>
#include <cstdint>

namespace pathTracing {

class BLNode;

// Represents an edge in the Dag.  For an edge, v -> w, v is the source, and
// w is the target.
class BLEdge {
public:
  enum EdgeType { NORMAL, BACKEDGE, SPLITEDGE, CALLEDGE, PHONYEDGE };

  // Constructor: Initializes an BLEdge with a source and target.
  BLEdge(BLNode* source, BLNode* target);

  // Returns the source/ target node of this edge.
  BLNode* getSource();
  const BLNode* getSource() const;
  BLNode* getTarget();
  const BLNode* getTarget() const;

  // Sets the type of the edge.
  EdgeType getType() const;

  // Gets the type of the edge.
  void setType(EdgeType type);

  // Returns the weight of this edge.  Used to decode path numbers to
  // sequences of basic blocks.
  unsigned getWeight() const;

  // Sets the weight of the edge.  Used during path numbering.
  void setWeight(unsigned weight);

  // Gets/sets the phony edge originating at the root.
  BLEdge* getPhonyRoot();
  const BLEdge* getPhonyRoot() const;
  void setPhonyRoot(BLEdge* phonyRoot);

  // Gets/sets the phony edge terminating at the exit.
  BLEdge* getPhonyExit();
  const BLEdge* getPhonyExit() const;
  void setPhonyExit(BLEdge* phonyExit);

  // Gets/sets the associated real edge if this is a phony edge.
//  BLEdge* getRealEdge();
//  void setRealEdge(BLEdge* realEdge);

  // Sets the target node of this edge.  Required to split edges.
  void setTarget(BLNode* node);

  // Get/set whether edge is in the spanning tree.
  bool isInSpanningTree() const;
  void setIsInSpanningTree(bool isInSpanningTree);

  // Get/ set whether this edge will be instrumented with a path number
  // initialization.
  bool isInitialization() const;
  void setIsInitialization(bool isInitialization);

  // Get/set whether this edge will be instrumented with a path counter
  // increment.  Notice this is incrementing the path counter
  // corresponding to the path number register.  The path number
  // increment is determined by getIncrement().
  bool isBLPathEnd() const;
  void setIsBLPathEnd(bool isBLPathEnd);

  // Get/set the path number increment that this edge will be instrumented
  // with.  This is distinct from the path counter increment and the
  // weight.  The counter increment counts the number of executions of
  // some path, whereas the path number keeps track of which path number
  // the program is on.
  int64_t getIncrement() const;
  void setIncrement(int64_t increment);

  // Get/set whether the edge has been instrumented.
  bool hasInstrumentation() const;
  void setHasInstrumentation(bool hasInstrumentation);

  // Returns the successor number of this edge in the source.
  unsigned getSuccessorNumber() const;

  bool isChordEdge() const;
  void setIsChordEdge(bool isChordEdge);

private:
  // Source node for this edge.
  BLNode* _source;

  // Target node for this edge.
  BLNode* _target;

  // Edge weight cooresponding to path number increments before removing
  // increments along a spanning tree. The sum over the edge weights gives
  // the path number.
  unsigned _weight;

  // Type to represent for what this edge is intended
  EdgeType _edgeType;

  // For backedges and split-edges, the phony edge which is linked to the
  // root node of the DAG. This contains a path number initialization.
  BLEdge* _phonyRoot;

  // For backedges and split-edges, the phony edge which is linked to the
  // exit node of the DAG. This contains a path counter increment, and
  // potentially a path number increment.
  BLEdge* _phonyExit;

  // If this is a phony edge, _realEdge is a link to the back or split
  // edge. Otherwise, this is null.
//  BLEdge* _realEdge;

  // The increment that the code will be instrumented with.
  int64_t _increment;

  // Whether this edge is in the spanning tree.
  bool _isInSpanningTree;

  // Whether this edge is a chord edge.
  bool _isChord;

  // Whether this edge is an initialiation of the path number.
  bool _isInitialization;

  // Whether this edge is a path counter increment.
  bool _isBLPathEnd;

  // Whether this edge has been instrumented.
  bool _hasInstrumentation;
};

} // end of namespace pathTracing
#endif
