#ifndef HPPTRACE_H
#define HPPTRACE_H
//#include "llvm/IR/InstrTypes.h"
//#include "llvm/IR/Instructions.h"
//#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Casting.h"
//#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <cstdint>
#include <sstream>
#include <fstream>
#include <stack>
#include <list>
#include <tr1/unordered_map>
//#include <map>

#include "../include/BLInstrumentation.h"
#include "../include/LogInterface.h"
#include "../include/DictInterface.h"
#include <climits>

using namespace llvm;
using std::tr1::unordered_map;

typedef std::vector<const BasicBlock*> BasicBlockPath;
typedef std::tr1::unordered_map<uint32_t, BasicBlockPath *> PathToBB; //add by srwu
typedef std::tr1::unordered_map<uint32_t, PathToBB> FunctionToPath;   //add by srwu

namespace {

	const unsigned NUM_BACKTRACE_LOWER_BOUND = 1;
	const unsigned NUM_BACKTRACE_HIGHER_BOUND = UINT_MAX; //UINT_MAX

class HPPNode;
typedef unordered_map<std::string, uint32_t> SigHash;
typedef unordered_map<uint32_t, uint32_t> PathMappingHash;

class Label {
	public:
		// this is used to make PathVisitState an abstract class
		virtual ~Label(){}
};

class ProcedureLabel : public Label {
	public:
		ProcedureLabel(uint32_t funcid) : funcID(funcid) {}
		uint32_t getFuncID() const{ return funcID; }
		virtual ~ProcedureLabel(){}
	private:
		const uint32_t funcID;
};

class BLPathLabel : public Label {
	public:
		BLPathLabel(uint32_t pathid) : pathID(pathid){}
		uint32_t getPathID() const{ return pathID; }
		virtual ~BLPathLabel(){}
	private:
		const uint32_t pathID;
};

class UnfinishedPathLabel : public Label {
	public:
		UnfinishedPathLabel(uint32_t watchPointid, int64_t ps) : watchPointID(watchPointid), pathState(ps) {}
		virtual ~UnfinishedPathLabel(){}
		uint32_t getWatchPointID() const{ return watchPointID; }
		int64_t getPathState() const{ return pathState; }
	private:
		const uint32_t watchPointID;
		const int64_t pathState;
};

class UntrackCallSiteLabel : public Label {
	public:
		UntrackCallSiteLabel(uint32_t label) : untrackCallIndex(label) {}
		virtual ~UntrackCallSiteLabel(){}
		uint32_t getUntrackCallIndex() const { return untrackCallIndex; }
	private:
		const uint32_t untrackCallIndex;
};

class TreeNode;
class DagNode;
class HPPNode {
	public:
		HPPNode(): nodeLabel(NULL) {}
		NodeType getType() const { return Type; }
		Label* getLabel() const { return nodeLabel; }
		void setLabel(Label* label) { nodeLabel = label; }
		void setType(NodeType T) { Type = T; }
		// release the dynamic allocated memory
		~HPPNode() { 
			if(nodeLabel)
				delete nodeLabel; 
		}
		uint32_t getFuncID() const;
		uint32_t getPathID() const;
		uint32_t getUntrackCallIndex() const;
		uint32_t getWatchPointID() const;
		int64_t getPathState() const;
	private:
		NodeType Type;
		Label* nodeLabel;
};

struct HPPEdge {
	uint32_t numOfRepeats;
	DagNode* targetNode;
};

class DagNode : public HPPNode {
	public:
		enum VisitState {UNVISITED, VISITED, EXPLORED};
		DagNode(): HPPNode(), ns(UNVISITED), count(0), indegree(0), height(0) {}
		std::vector<HPPEdge>& getPreds() { return preds; }
		std::vector<HPPEdge>& getSuccs() { return succs; }
		VisitState getVisitState() { return ns; }
		void setVisitState(VisitState state) { ns = state; }
		std::string toString();
		void printNode();
		uint64_t getCount() {return count;}
		void setCount(uint64_t num) {count = num;}
		uint32_t getIndegree() {return indegree;}
		void setIndegree(uint32_t ind) {indegree = ind;}
		uint32_t getHeight() {return height;}
		void setHeight(uint32_t hei) {height = hei;}
	private:
		std::vector<HPPEdge> preds;
		std::vector<HPPEdge> succs;
		VisitState ns;
		uint64_t count;
		uint32_t indegree;
		uint32_t height;
};

class HPPDag {
	public:
		HPPDag(): numOfNodes(0), numOfEdges(0) {}
		DagNode* getRootNode() {return rootNode;}
		void setRootNode(DagNode* root) {rootNode = root;}
		std::vector<DagNode*>& getLeafNodes() {return leafNodes;}
		std::vector<DagNode*>& getProcNodes() {return procNodes;}
		std::vector<DagNode*>& getPathNodes() {return pathNodes;}
		std::vector<DagNode*>& getCallsiteNodes() {return callsiteNodes;}
		void printBackTraces(uint32_t tracedProcID);
		void calculateProfiles();
		void calculateHeight();
		uint32_t getNumOfNodes() {return numOfNodes;}
		uint32_t getNumOfEdges() {return numOfEdges;}
		void increNumOfNodes() {++numOfNodes;}
		void increNumOfEdges() {++numOfEdges;}
	private:
		DagNode* rootNode;
		std::vector<DagNode*> leafNodes;
		std::vector<DagNode*> procNodes;
		std::vector<DagNode*> pathNodes;
		std::vector<DagNode*> callsiteNodes;
		uint32_t numOfNodes;
		uint32_t numOfEdges;
};

const size_t PROC_NODE_DATA_SIZE = 8;
const size_t PATH_NODE_DATA_SIZE = 8;
const size_t UPATH_NODE_DATA_SIZE = 16;
const size_t INDETER_CS_NODE_DATA_SIZE = 4;
const size_t UNTRACK_CS_NODE_DATA_SIZE = 8;

class TreeNode : public HPPNode {
	public:
		// null constructor for path node
		TreeNode(): HPPNode(), isNew(false), parentFuncID(0), nodeData(NULL) {}
		// init funcId (procLabel) && init type
		TreeNode(NodeType T, uint32_t funcId);
		// init call site node
		TreeNode(NodeType T): isNew(false), parentFuncID(0), nodeData(NULL) { setType(T); }
		// add a dag node as its child
    /*
    ~TreeNode() {
      if (nodeData != NULL) {
        delete[] nodeData;
      }
    }
    */
		void addChild(TreeNode* child);
		// get the parent procedure call node's function ID if
		//	this node is a path node
		uint32_t getParentFuncID() { assert(getType() == NT_PathNode); return parentFuncID; }
		std::vector<NodeSignature>& getChildrenSig() { return childrenSig; }
		const std::vector<NodeSignature>& getChildrenSig() const { return childrenSig; }
		uint32_t getChildrenNum() const { return childrenSig.size(); }
		// retrieve the node signature
		// return isNewNode
		bool isNewNode() { return isNew; }
		// set isNewNode as true
		void setIsNewNode() { isNew= true; }
		// set the path label as well as the node text as well as the node type
		void setBLPath(uint32_t funcId, uint32_t pathId);
		// set the index of the cs node as well as the node text
		void setUntrackCallIndex(uint32_t index);
		// set the path label as well as the node type
		void setUnfinishedPath(uint32_t watchpointId, int64_t pathState);
		void constructNodeText(std::string& str);
		void writeCompressedDagGrammar();
		uint32_t checkDuplication();
    void createNodeData();

    char* nodeData;
    size_t nodeDataSize;

	private:
		std::vector<NodeSignature> childrenSig;
		bool isNew;
		uint32_t parentFuncID;
		uint32_t lastChildSig;
};

// TEST
class hashFunc {
  public:
    size_t operator() (const TreeNode* node) const {
      NodeType type = node->getType();
      size_t result = 0;
      hash_combine(result, static_cast<int>(type));

      // get node type, hash
      switch (type) {
        case NT_ProcedureNode:
          {
            // nodeText <ProcID, childrenSig>
            hash_combine(result, node->getFuncID());
            break;
          }
        case NT_PathNode:
          {
            // nodeText <PathID, childrenSig>
            hash_combine(result, node->getPathID());
            break;
          }
        case NT_IndeterCallSiteNode:
          {
            // nodeText <childrenSig>
            break;
          }
        case NT_UntrackCallSiteNode:
          {
            // nodeText <callsite index, childrenSig>
            hash_combine(result, node->getUntrackCallIndex());
            break;
          }
        case NT_UnfinishedPathNode:
          {
            // nodeText <watchpointID, PathState, childrenSig>
            hash_combine(result, node->getWatchPointID());
            hash_combine(result, node->getPathState());
            break;
          }
        default:
          assert("unidentified node type");
      }
      hash_combine(result, node->getChildrenNum());
      return result;
    }
  private:
    template <class T>
    static inline void hash_combine(size_t& seed, const T& v) {
      std::hash<T> hasher;
      seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }
};

class treeEqual {
  public:
    bool operator() (const TreeNode* node1, const TreeNode* node2) const {
      // compare node type
      /*
      if (node1->getType() != node2->getType()) { return false; }
      if (node1->getChildrenNum() != node2->getChildrenNum()) { return false; }
      switch (node1->getType()) {
        case NT_ProcedureNode:
          {
            if (node1->getFuncID() != node2->getFuncID()) { return false; }
            break;
          }
        case NT_PathNode:
          {
            if (node1->getPathID() != node2->getPathID()) { return false; }
            break;
          }
        case NT_IndeterCallSiteNode:
          {
            break;
          }
        case NT_UntrackCallSiteNode:
          {
            if (node1->getUntrackCallIndex() != node2->getUntrackCallIndex()) { return false; }
            break;
          }
        case NT_UnfinishedPathNode:
          {
            if (node1->getWatchPointID() != node2->getWatchPointID()) { return false; }
            if (node1->getPathState() != node2->getPathState()) { return false; }
            break;
          }
        default:
          assert("unidentified node type");
      }
      */
     
      if (memcmp(node1->nodeData, node2->nodeData, node1->nodeDataSize) != 0) {
        return false;
      }
      char* children1 = (char *)node1->getChildrenSig().data();
      char* children2 = (char *)node2->getChildrenSig().data();
      size_t size = node1->getChildrenNum() * sizeof(NodeSignature);
      return memcmp(children1, children2, size) == 0;
      /*
      for (uint32_t i = 0; i < node1->getChildrenNum(); ++i) {
        if (node1->getChildrenSig()[i] != node2->getChildrenSig()[i]) { return false; }
      }

      // compare each child
      return true;
      */
    }
};

// TEST
typedef unordered_map<TreeNode*, uint32_t, hashFunc, treeEqual> SigHash2;
SigHash2* procHash2;
SigHash2* pathHash2;
SigHash2* uPathHash2;
SigHash2* indeterCSHash2;
SigHash2* untrackCSHash2;

// global variables in the anoymous namespace
SigHash* procHash;
SigHash* pathHash;
SigHash* callsiteHash;

std::ofstream traceDict;
//std::ofstream dictIndex;
std::vector<HPPNode*>* dagNodes;

// init & cleanup operations on global variables
void initDictGlobals(std::string dictPath);
void cleanupDict();
void cleanupDags();

void initDictGlobals(std::string dictPath) { 

	std::string traceDictName = dictPath + "/traceDict";
//	std::string dictIndexName = dictPath + "/traceIndex";
	traceDict.open(traceDictName.c_str(), std::ios::out | std::ios::binary);
//	dictIndex.open(dictIndexName.c_str(), std::ios::out | std::ios::binary);
	dagNodes = new std::vector<HPPNode*>();

	procHash = new SigHash();
	pathHash = new SigHash();
	callsiteHash = new SigHash();
  // TEST
	procHash2 = new SigHash2();
	pathHash2 = new SigHash2();
  uPathHash2 = new SigHash2();
  indeterCSHash2 = new SigHash2();
  untrackCSHash2 = new SigHash2();
}

void cleanupDict() {
  llvm::outs() << "Num of dag nodes: " << procHash->size() + pathHash->size() + callsiteHash->size() << '\n';
  // TEST
  int test1 [3] = {10, 0, 21};
  int test2 [3] = {10, 0, 23};
  std::string str1 = std::string((char*)test1, 3 * sizeof(int));
  std::string str2 = std::string((char*)test2, 3 * sizeof(int));
  procHash->insert(std::make_pair(str1, 999));
  SigHash::iterator it = procHash->find(str2);
  if (it != procHash->end()) {
    llvm::outs() << "str1 and str2 duplicate. " << it->second <<" \n";
  } else {
    llvm::outs() << "str1 and str2 different\n";
  }
	delete procHash;
	delete pathHash;
	delete callsiteHash;
  // TEST
  llvm::outs() << "Num of dag nodes: " << procHash2->size() + pathHash2->size() + uPathHash2->size() 
    + indeterCSHash2->size() + untrackCSHash2->size() << '\n';
	delete procHash2;
	delete pathHash2;
	delete uPathHash2;
	delete indeterCSHash2;
	delete untrackCSHash2;

	traceDict.close();
//	dictIndex.close();
}

void cleanupDags() {
	delete dagNodes;
}

class HPPTrace : public ModulePass {

	// inner class used to traverse instructions of BasicBlockPath
	class ConstBBPathInsnIterator {
		public:
			explicit ConstBBPathInsnIterator(const BasicBlockPath& bbPath);
			const Instruction* getNextInsn();
		private:
			const BasicBlockPath& path;
			BasicBlockPath::const_iterator nextBBIter;
			BasicBlockPath::const_iterator bbEnd;
			BasicBlock::const_iterator nextInsnIter;
			BasicBlock::const_iterator insnEnd;
	};

	public:
		static char ID; // Pass identification, replacement for typeid
		HPPTrace() : ModulePass(ID), hppRecoveryDags(), wppRecoveryDags(), //pathPool(NULL),
			enterProcCounter(0), exitProcCounter(0), pathCounter(0),
			path2TrackCSCounter(0), path2IndeterCSCounter(0), path2UntrackCSCounter(0),
			enterIndeterCSCounter(0), exitIndeterCSCounter(0), enterUntrackCSCounter(0), 
			exitUntrackCSCounter(0), unfinishedPathCounter(0), currSig(0) {}
	private:

//		std::ofstream pathDict;
//		std::ofstream proCSDict;

//		LLVMContext* context;
		virtual bool runOnModule(Module &M);
		void showStatistics();
		void printTraces();
		//void wppCreateDictionary(Module &M);
		void createDAGfromHPPTree();
		void loadHPPTreeTraces();
		std::vector<pathTracing::BLDag*> hppRecoveryDags;
		std::vector<pathTracing::BLWPPRecoveryDag*> wppRecoveryDags;
		std::vector<PathMappingHash*> wppPathMappingHash;
		// Two-dimensional array of type vector<llvm::BasicBlock*>*
		//	the first dimension is the function ID
		//	the second dimension is the path ID
		//	The array element is a pointer pointing to a sequence(vector) of BasicBlock*
		//		that the corresponding path traverses
		//BasicBlockPath*** pathPool;  //comment by srwu
		FunctionToPath pathPoolMap;    //add by srwu
		void initHPPPathRecovery(Module& M);
		void printTrace(std::ifstream& fileHandle);
		void collectStatistics(std::ifstream& fileHandle);
		void getExercisedProcsInRange(Module& M);
		void printBackTraceFromRawTrace(std::ifstream& fileHandle, uint32_t tracedProcID);
		std::vector<uint32_t>* generateBacktraceTestCases(unsigned repeats);
		void queryHppBackTrace(unsigned repeats);
		void queryBLPTBackTrace(unsigned repeats);
		void getPathProfiles();
		void getBenchInfo(Module& M);


		const BasicBlockPath* retrievePath(uint32_t functionID, uint32_t pathID);
		TreeNode* buildSerializedDag(std::ifstream& fileHandle ); 
		void loadHPPTree(std::ifstream& fileHandle ); 
		//uint32_t buildDagFromWPP(std::ifstream& fileHandle);
		uint32_t retrieveOriginalBLPathID(uint32_t funcID, const std::vector<uint32_t>& paths);
//		HPPNode* checkDuplication(TreeNode* node, SigHash& sigHashSet);
		bool isCallPath(int funcID, int pathID);
		HPPDag* createDag();
		void printDict();



		// trace statistics
		uint64_t enterProcCounter;
		uint64_t exitProcCounter;
		uint64_t pathCounter;
		uint64_t path2TrackCSCounter;
		uint64_t path2IndeterCSCounter;
		uint64_t path2UntrackCSCounter;
		uint64_t enterIndeterCSCounter;
		uint64_t exitIndeterCSCounter;
		uint64_t enterUntrackCSCounter;
		uint64_t exitUntrackCSCounter;
		uint64_t unfinishedPathCounter;
		
		uint32_t currSig;

};
} // end anonymous namespace

#endif
