#ifndef DICTINTERFACE_H
#define DICTINTERFACE_H

const unsigned NodeTypeSize = 1; //byte
const unsigned NumOfChildrenSize = 4; //byte

enum NodeType {
	NT_ProcedureNode = 0,
	NT_PathNode,
	NT_UnfinishedPathNode,
	NT_UntrackCallSiteNode,
	NT_IndeterCallSiteNode,
	NT_PhonyNode
};

const char Nodes[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};

class NodeSignature {
  public:
    uint32_t numOfRepeats;
    uint32_t signature;
    bool operator!=(const NodeSignature& sig) const {
      return (numOfRepeats != sig.numOfRepeats || signature != sig.signature);
    }
};
#endif
