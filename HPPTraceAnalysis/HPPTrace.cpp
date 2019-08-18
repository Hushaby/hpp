#include "HPPTrace.h"
#include <utility>
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Threading.h"
#include <unistd.h>
#include <google/profiler.h>
#include <queue>
#include <cmath>

char HPPTrace::ID = 0;
bool isDebug = false;
static RegisterPass<HPPTrace> X("HPPTrace", "HPP Trace Analysis");
cl::opt<std::string> HPPDAGTracePath("trace-path", cl::desc("Specify the path of serialized HPPDAG file"),
		cl::value_desc("HPPDAG trace file path"));
cl::opt<std::string> ProcList("proc-list", cl::desc("Specify the path of the executed procedure list"),
		cl::value_desc("procedure list path"));
cl::list<std::string> RawTraceFiles("trace-files", cl::value_desc("list"), cl::desc("A list of paths of blpt or hppTree trace file folders to be processed"), cl::CommaSeparated);

cl::opt<bool> ShowStat("show-statistics", cl::desc("show the statistics of the trace log"), cl::init(false));
cl::opt<bool> GetExercisedProcList("get-proc-list", cl::desc("get the list of exercised procedures"), cl::init(false));
cl::opt<bool> GetExecutedPaths("get-exec-paths", cl::desc("get the set of executed BL paths"), cl::init(false));
cl::opt<bool> PrintTrace("print-traces", cl::desc("print out the trace logs"), cl::init(false));
cl::opt<bool> CreateSerializedHPPDAG("hpp-create-serialized-dag", cl::desc("generate the HPPDAG serialized form from the raw HPP trace"), cl::init(false));
cl::opt<bool> TimeHPPTreeIO("time-hpptree-io", cl::desc("Load the raw HPPTree trace but do nothing"), cl::init(false));
cl::opt<bool> TimeDAGConstructionNoIO("time-serialized-dag-no-io", cl::desc("generate the HPPDAG serialized form from the raw HPP trace without IO output"), cl::init(false));
cl::opt<bool> CreateDag("create-dag", cl::desc("create the trace dag from the compacted trace files"), cl::init(false));
cl::opt<bool> GetNumOfDAGNodes("count-dag-nodes", cl::desc("get the number of HPPDAG nodes from the serialized dag trace files"), cl::init(false));
cl::opt<bool> GetPathProfiles("get-profiles", cl::desc("get the path profiles from the HPPDag"), cl::init(false));
cl::opt<bool> BLPTBacktrace("blpt-backtrace", cl::desc("perform backtrace query on blpt trace files"), cl::init(false));
cl::opt<bool> HPPBackTrace("hpp-backtrace", cl::desc("create the hpp backtrace from hppdag"), cl::init(false));
cl::opt<unsigned> QueryNum("query-num", cl::desc("the number of backtrace queries to be performed"), cl::init(100));
cl::opt<bool> GetBenchInfo("get-bench-info", cl::desc("get the statics statistics of the benchmark"), cl::init(false));

void* intervalCheck(void* abc) {
	while (true) {
		llvm::outs()<<"==============================================\n";
		if (dagNodes)
			llvm::outs()<<"dagNodes->size(): "<<dagNodes->size()<<'\n';
    // DEBUG
		if (procHash2)
			llvm::outs()<<"procHash->size(): "<<procHash2->size()<<'\n';
		if (pathHash2)
			llvm::outs()<<"pathHash->size(): "<<pathHash2->size()<<'\n';
    /*
		if (callsiteHash2)
			llvm::outs()<<"callsitecHash->size(): "<<callsiteHash2->size()<<'\n';
      */
		llvm::outs()<<"==============================================\n\n\n";
		sleep(60);
	}
}

bool HPPTrace::runOnModule(Module &M) {

	DEBUG(dbgs()
        << "********************************************\n"
        << "********************************************\n"
        << "**                                        **\n"
        << "**          HPP Trace Analysis            **\n"
        << "**                                        **\n"
        << "********************************************\n"
        << "********************************************\n");
  //pthread_t timerThread;
  //pthread_create(&timerThread, NULL, &intervalCheck, NULL);
	Function *Main = M.getFunction("main");
	if (!Main) {
		errs() << "WARNING: no main function is found!\n";
		return false;
	}

#ifdef CPU_PROFILE
	ProfilerStart("CPU_prof.prof");
#endif
	// dipatch the task to the corresponding handler
	if (ShowStat) {
		showStatistics();
	}
	if (GetExercisedProcList) {
		getExercisedProcsInRange(M);
	}
	if (PrintTrace) {
		printTraces();
	}
	if (CreateSerializedHPPDAG) {
		clock_t start = clock();
		createDAGfromHPPTree();
		errs()<<"Serialized DAG construction time: "<<(double)(clock()- start)/CLOCKS_PER_SEC<<'\n';
	}
	if (TimeHPPTreeIO) {
		clock_t start = clock();
		loadHPPTreeTraces();
		errs()<<"HPPTree load time: "<<(double)(clock()- start)/CLOCKS_PER_SEC<<'\n';
	}
	if (TimeDAGConstructionNoIO) {
		clock_t start = clock();
		//createDAGfromHPPTreeNoIO();
		errs()<<"Serialized DAG construction time Without IO: "<<(double)(clock()- start)/CLOCKS_PER_SEC<<'\n';
	}
	if (CreateDag) {
		HPPDag* dag = createDag();
		errs()<<"Number of Nodes in HPPDAG: "<<dag->getNumOfNodes()<<'\n';
	}
	if (GetNumOfDAGNodes) {
		HPPDag* dag = createDag();
		errs()<<"Number of Nodes in HPPDAG: "<<dag->getNumOfNodes()<<'\n';
	}
	if (BLPTBacktrace) {
		queryBLPTBackTrace(QueryNum);
	}
	if (HPPBackTrace) {
		queryHppBackTrace(QueryNum);
	}
	if (GetPathProfiles) {
		getPathProfiles();
	}
	if (GetBenchInfo) {
		getBenchInfo(M);
	}
#ifdef CPU_PROFILE
	ProfilerStop();
#endif
	return false;
}

struct ProcProfile {
	int procID;
	int count;
};

bool procProfileComp (ProcProfile a, ProcProfile b) {	return a.count < b.count; }

void HPPTrace::queryHppBackTrace(unsigned repeats) {
	std::vector<uint32_t> *testCases = generateBacktraceTestCases(repeats);
	// create the dag and record the creation time
	outs()<<"start loading dag ...\n";
	clock_t dag_createion_start = clock();
	HPPDag* dag = createDag();
	clock_t dag_creation_end = clock();

	outs()<<"start backtrace query ...\n";
	clock_t query_start = clock();
	for (unsigned i=0; i<testCases->size(); ++i) {
		outs()<<"\tProcID to be backtraced: "<<testCases->at(i)<<'\n';
//		errs()<<"ProcID to be backtraced: "<<testCases[i]<<'\n';
		dag->printBackTraces(testCases->at(i));
	}
	clock_t query_end = clock();
	
	// output the result
	errs()<<"Load time: "<<(double)(dag_creation_end - dag_createion_start)/CLOCKS_PER_SEC<<'\n';
	errs()<<"Query time: "<<(double)(query_end - query_start)/CLOCKS_PER_SEC<<'\n';
}

// write a file procList which contains the list
// of procedure ids that have been exercised in the trace files
void HPPTrace::getExercisedProcsInRange(Module& M) {
	unsigned count = 0;
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration())
			continue;
		++count;
	}
	outs()<<"Num of procedures: "<<count<<'\n';
	unsigned* procCount = new unsigned[count];
	for (unsigned i=0; i<count; ++i)
		procCount[i]=0;

	for (unsigned i = 0; i != RawTraceFiles.size(); ++i) {
		// processing each thread-local trace file
		unsigned traceNum = 0;
		while (true) {
			std::stringstream sstm;
			sstm << RawTraceFiles[i] << "/trace" << traceNum << ".log";
			std::ifstream fileHandle(sstm.str().c_str(), std::ios::in | std::ios::binary);
			if (fileHandle.is_open()) {
				outs()<<"get procedure call counters on "<<sstm.str()<<'\n';

				char eventId;
				char funcIdBuf[FuncIDSize];
				char pathIdBuf[PathIDSize];
				char indexBuf[UntrackCallIndexSize];
				char watchpointIdBuf[WatchPointIDSize];
				char pathStateBuf[PathStateSize];
				uint32_t funcID;

				while(fileHandle.read(&eventId, EventIDSize)){
					if (eventId == Events[ENTER_PROC]) {
						fileHandle.read(funcIdBuf, FuncIDSize);
						if (fileHandle.gcount() != FuncIDSize) {
							errs()<<"ERROR: fail reading funcID\n";
							return;
						}
						funcID = *((uint32_t*)funcIdBuf);
						++procCount[funcID];
					} else if (eventId == Events[EXIT_PROC]) {
					} else if (eventId == Events[PATH]) {
						fileHandle.read(pathIdBuf, PathIDSize);
						if (fileHandle.gcount() != PathIDSize) {
							errs()<<"ERROR: fail reading pathID\n";
							return;
						}
					} else if (eventId == Events[PATH2TRACKCS]) {
						fileHandle.read(pathIdBuf, PathIDSize);
						if (fileHandle.gcount() != PathIDSize) {
							errs()<<"ERROR: fail reading pathID\n";
							return;
						}
					} else if (eventId == Events[PATH2INDETERCS]) {
						fileHandle.read(pathIdBuf, PathIDSize);
						if (fileHandle.gcount() != PathIDSize) {
							errs()<<"ERROR: fail reading pathID\n";
							return;
						}
					} else if (eventId == Events[PATH2UNTRACKCS]) {
						fileHandle.read(pathIdBuf, PathIDSize);
						if (fileHandle.gcount() != PathIDSize) {
							errs()<<"ERROR: fail reading pathID\n";
							return;
						}
					} else if (eventId == Events[ENTER_INDETER_CS]) {
					} else if (eventId == Events[EXIT_INDETER_CS]) {
					} else if (eventId == Events[ENTER_UNTRACK_CS]) {
					} else if (eventId == Events[EXIT_UNTRACK_CS]) {
						fileHandle.read(indexBuf, UntrackCallIndexSize);
						if (fileHandle.gcount() != UntrackCallIndexSize) {
							errs()<<"ERROR: fail reading untrack call index\n";
							return;
						}
					} else if (eventId == Events[UNFINISHED_PATH]) {
						fileHandle.read(watchpointIdBuf, WatchPointIDSize);
						if (fileHandle.gcount() != WatchPointIDSize) {
							errs()<<"ERROR: fail reading watch point ID\n";
							return;
						}
						fileHandle.read(pathStateBuf, PathStateSize);
						if (fileHandle.gcount() != PathStateSize) {
							errs()<<"ERROR: fail reading path state\n";
							return;
						}
					} else {
						errs()<<"ERROR: unidentified log content\n";
						return;
					}
				} 
				traceNum++;
				fileHandle.close();
			} else {
				outs()<<"Finish processing all log files\nNum of thread processed: "<<traceNum<<'\n';
				break;
			}
		}
	}
	std::ofstream procList;
	std::string procListFileName = "./procList";
	procList.open(procListFileName.c_str(), std::ios::out | std::ios::binary);
	for (uint32_t i=0; i<count; ++i) {
		if (procCount[i] != 0) {
			errs()<<"Proc: "<<i<<" ,Count: "<<procCount[i]<<'\n';
			procList.write((char*)(&i), sizeof(uint32_t));
			procList.write((char*)(&procCount[i]), sizeof(uint32_t));
		}
	}
	procList.close();
}

void HPPTrace::showStatistics() {
	for (unsigned i = 0; i != RawTraceFiles.size(); ++i) {
		// processing each thread-local trace file
		unsigned traceNum = 0;
		while (true) {
			std::stringstream sstm;
			sstm << RawTraceFiles[i] << "/trace" << traceNum << ".log";
			std::ifstream logFile (sstm.str().c_str(), std::ios::in | std::ios::binary);
			if (logFile.is_open()) {
				outs()<<"Collect statistics on "<<sstm.str()<<'\n';
				collectStatistics(logFile);
				traceNum++;
				logFile.close();
			} else {
				outs()<<"Finish processing all log files in "<<RawTraceFiles[i]<<"\nNum of thread processed: "<<traceNum<<'\n';
				break;
			}
		}
	}

	//output the statistics
	errs()<<"\nTrace statistics:\n";
	errs()<<"Num of ENTER_PROC: "<<enterProcCounter<<'\n';
	errs()<<"Num of EXIT_PROC: "<<exitProcCounter<<'\n';
	errs()<<"Num of PATH: "<<pathCounter<<'\n';
	errs()<<"Num of PATH2TRACKCS: "<<path2TrackCSCounter<<'\n';
	errs()<<"Num of PATH2UNTRACKCS: "<<path2UntrackCSCounter<<'\n';
	errs()<<"Num of PATH2INDETERCS: "<<path2IndeterCSCounter<<'\n';
	errs()<<"Num of ENTER_UNTRACK_CS: "<<enterUntrackCSCounter<<'\n';
	errs()<<"Num of EXIT_UNTRACK_CS: "<<exitUntrackCSCounter<<'\n';
	errs()<<"Num of ENTER_INDETER_CS: "<<enterIndeterCSCounter<<'\n';
	errs()<<"Num of EXIT_INDETER_CS: "<<exitIndeterCSCounter<<'\n';
	errs()<<"Num of UNFINISHED_PATH: "<<unfinishedPathCounter<<'\n';
}

void HPPTrace::printTraces() {
	for (unsigned i = 0; i != RawTraceFiles.size(); ++i) {
		// processing each thread-local trace file
		unsigned traceNum = 0;
		while (true) {
			std::stringstream sstm;
			sstm << RawTraceFiles[i] << "/trace" << traceNum << ".log";
			std::ifstream logFile (sstm.str().c_str(), std::ios::in | std::ios::binary);
			if (logFile.is_open()) {
				outs()<<"print trace: "<<sstm.str()<<'\n';
				printTrace(logFile);
				traceNum++;
				logFile.close();
			} else {
				outs()<<"Finish processing all log files in "<<RawTraceFiles[i]<<"\nNum of thread processed: "<<traceNum<<'\n';
				break;
			}
		}
	}
}

void HPPTrace::createDAGfromHPPTree() {
	// init the global variables on node dictionary
	// create the traceDict file in the current folder
	initDictGlobals(".");
	TreeNode* rootNode = new TreeNode(NT_PhonyNode);
	// processing each trace folder
	for (unsigned i = 0; i != RawTraceFiles.size(); ++i) {
		TreeNode* executionNode = new TreeNode(NT_PhonyNode);
		// processing each thread-local trace file
		unsigned traceNum = 0;
		while (true) {
			std::stringstream sstm;
			sstm << RawTraceFiles[i] << "/trace" << traceNum << ".log";
			std::ifstream logFile (sstm.str().c_str(), std::ios::in | std::ios::binary);
			if (logFile.is_open()) {
				outs()<<"Construct HPPDAG on "<<sstm.str()<<'\n';
				TreeNode* threadNode = buildSerializedDag(logFile);
				executionNode->addChild(threadNode);
				traceNum++;
				logFile.close();
			} else {
				outs()<<"Finish processing all log files in "<<RawTraceFiles[i]<<"\nNum of thread processed: "<<traceNum<<'\n';
				break;
			}
		}
		rootNode->addChild(executionNode);
	}
	// write the rootNode's data to the dict file
	rootNode->writeCompressedDagGrammar();
	// cleanup the nodes dictionary
	cleanupDict();
}

void HPPTrace::loadHPPTreeTraces() {
	// processing each trace folder
	for (unsigned i = 0; i != RawTraceFiles.size(); ++i) {
		// processing each thread-local trace file
		unsigned traceNum = 0;
		while (true) {
			std::stringstream sstm;
			sstm << RawTraceFiles[i] << "/trace" << traceNum << ".log";
			std::ifstream logFile (sstm.str().c_str(), std::ios::in | std::ios::binary);
			if (logFile.is_open()) {
				outs()<<"Load HPPTree on "<<sstm.str()<<'\n';
				loadHPPTree(logFile);
				traceNum++;
				logFile.close();
			} else {
				outs()<<"Finish processing all log files in "<<RawTraceFiles[i]<<"\nNum of thread processed: "<<traceNum<<'\n';
				break;
			}
		}
	}
}

void HPPTrace::printDict() {
	std::ifstream dictHandle (HPPDAGTracePath.c_str(), std::ios::in | std::ios::binary);

	char nodeType_c;
	while (dictHandle.read(&nodeType_c, NodeTypeSize)) {
		outs()<<"nodeType: "<<(int)nodeType_c<<",";
		// set node type & content
		if (nodeType_c == Nodes[NT_ProcedureNode]) {
			uint32_t funcID;
			dictHandle.read((char*)&funcID, FuncIDSize);
			outs()<<"funcID: "<<funcID<<",";
		} else if (nodeType_c == Nodes[NT_PathNode]) {
			uint32_t pathID;
			dictHandle.read((char*)&pathID, PathIDSize);
			outs()<<"pathID: "<<pathID<<",";
		} else if (nodeType_c == Nodes[NT_UntrackCallSiteNode]) {
			uint32_t index;
			dictHandle.read((char*)&index, UntrackCallIndexSize);
			outs()<<"untrack_cs_index: "<<index<<",";
		} else if (nodeType_c == Nodes[NT_IndeterCallSiteNode]) {
		} else if (nodeType_c == Nodes[NT_UnfinishedPathNode]) {
			uint32_t watchpointID;
			int64_t pathState;

			dictHandle.read((char*)&watchpointID, WatchPointIDSize);
			dictHandle.read((char*)&pathState, PathStateSize);
			outs()<<"watchpoint: "<<watchpointID<<",pathState: "<<pathState<<",";
		} else {
			assert("unidentified node type");
		}
		// set parent-children link
		uint32_t numOfChildren;	
		dictHandle.read((char*)&numOfChildren, NumOfChildrenSize);
		outs()<<"num_of_children: "<<numOfChildren<<",";
		for (unsigned i=0; i<numOfChildren; ++i) {
			uint32_t repeat;
			uint32_t sig;
			dictHandle.read((char*)&repeat, sizeof(uint32_t));
			dictHandle.read((char*)&sig, sizeof(uint32_t));

			outs()<<"[ "<<repeat<<", "<<sig<<" ], ";
		}
		outs()<<'\n';
	}
}

HPPDag* HPPTrace::createDag() {
	std::string dagPath = HPPDAGTracePath;
	std::ifstream dictHandle (dagPath.c_str(), std::ios::in | std::ios::binary);

	if (!dictHandle.is_open()) {
		errs()<<dagPath<<" is not found\n";
	}
	HPPDag* dag = new HPPDag();

	std::vector<DagNode*> sig2Node;
	char nodeType_c;
	while (dictHandle.read((char*)&nodeType_c, NodeTypeSize)) {
		// create a node
		DagNode* node = new DagNode();
		dag->increNumOfNodes();
		sig2Node.push_back(node);
		// set node type & content
		if (nodeType_c == Nodes[NT_ProcedureNode]) {
			node->setType(NT_ProcedureNode);
			uint32_t funcID;
			dictHandle.read((char*)&funcID, FuncIDSize);
			Label* procLabel = new ProcedureLabel(funcID);
			node->setLabel(procLabel);
			dag->getProcNodes().push_back(node);
		} else if (nodeType_c == Nodes[NT_PathNode]) {
			node->setType(NT_PathNode);
			uint32_t pathID;
			dictHandle.read((char*)&pathID, PathIDSize);
			Label* blPathLabel = new BLPathLabel(pathID);
			node->setLabel(blPathLabel);
			dag->getPathNodes().push_back(node);
		} else if (nodeType_c == Nodes[NT_UntrackCallSiteNode]) {
			node->setType(NT_UntrackCallSiteNode);
			uint32_t index;
			dictHandle.read((char*)&index, UntrackCallIndexSize);
			Label* indexLabel = new UntrackCallSiteLabel(index);
			node->setLabel(indexLabel);
			dag->getCallsiteNodes().push_back(node);
		} else if (nodeType_c == Nodes[NT_IndeterCallSiteNode]) {
			node->setType(NT_IndeterCallSiteNode);
			dag->getCallsiteNodes().push_back(node);
		} else if (nodeType_c == Nodes[NT_UnfinishedPathNode]) {
			node->setType(NT_UnfinishedPathNode);
			uint32_t watchpointID;
			int64_t pathState;

			dictHandle.read((char*)&watchpointID, WatchPointIDSize);
			dictHandle.read((char*)&pathState, PathStateSize);

			Label* unfinishedPathLabel = new UnfinishedPathLabel(watchpointID, pathState);
			node->setLabel(unfinishedPathLabel);
			dag->getPathNodes().push_back(node);
		} else if (nodeType_c == Nodes[NT_PhonyNode]) {
			node->setType(NT_PhonyNode);
		} else {
			assert("unidentified node type");
		}
		// set parent-children link
		uint32_t numOfChildren;	
		dictHandle.read((char*)&numOfChildren, NumOfChildrenSize);
		node->getSuccs().reserve(numOfChildren);
		// if the node has no children, add to the list of leaf nodes
		if (numOfChildren == 0) {
			dag->getLeafNodes().push_back(node);
		}

		for (unsigned i=0; i<numOfChildren; ++i) {
			uint32_t repeat;
			uint32_t sig;
			dictHandle.read((char*)&repeat, sizeof(uint32_t));
			dictHandle.read((char*)&sig, sizeof(uint32_t));

			// set children
			HPPEdge edge;
			edge.numOfRepeats = repeat;
			edge.targetNode = sig2Node[sig];
			node->getSuccs().push_back(edge);

			// set parent
			edge.targetNode = node;
			sig2Node[sig]->getPreds().push_back(edge);

			dag->increNumOfEdges();
		}
	}
	// root node should be the last node in the traceDict
	dag->setRootNode(sig2Node.back());
	return dag;
}

void HPPTrace::initHPPPathRecovery(Module& M) {
	uint32_t functionID = 0;
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration())
			continue;
		DEBUG(dbgs() << "Function: " << F->getName() << "\n");

		pathTracing::BLDag* dag = pathTracing::generateHPPPathNumberingDag(F);

		this->hppRecoveryDags.push_back(dag);
		++functionID;
	}
}

void printTopNodes(HPPDag* dag, uint32_t topN) {
	class CompareDagNode {
		public:
			bool operator()(DagNode* n1, DagNode* n2) // Returns true if n1 has smaller repeats than n2
			{
				if (n1->getCount() > n2->getCount())
					return true;
				else
					return false;
			}
	};
	std::priority_queue<DagNode*, std::vector<DagNode*>, CompareDagNode> pq;
	for (uint32_t i=0; i<dag->getProcNodes().size(); ++i) {
		pq.push(dag->getProcNodes()[i]);
		if (pq.size() > topN) {
			pq.pop();
		}
	}
	uint32_t i = topN;
	while (!pq.empty()) {
		DagNode* node = pq.top();
		/*
		errs()<<i<<": "<<node->toString()
			<<", count: "<<node->getCount()<<", height: "<<node->getHeight()<<'\n';
			*/
		pq.pop();
		--i;
	}
}

void outputPathProfile(HPPDag* dag) {
	std::string pathProfileName="pathProfiles";
	std::ofstream pathProfiles;
	pathProfiles.open(pathProfileName.c_str(), std::ios::out);
	int count;
	for (uint32_t i=0; i<dag->getProcNodes().size(); ++i) {
		count = dag->getProcNodes()[i]->getCount();
		//errs()<<count<<",";
		pathProfiles << count <<'\t';
	}
}

void HPPTrace::getPathProfiles() {
	HPPDag* dag = createDag();
	uint32_t nodeSize = dag->getProcNodes().size() + dag->getPathNodes().size() + dag->getCallsiteNodes().size();
	errs()<<"Number of nodes: "<<nodeSize<<'\n';
	clock_t profile_calculation_start = clock();
	dag->calculateProfiles();
	clock_t profile_calculation_end = clock();

	// output the result
	errs()<<"Profile calculation time: "<<(double)(profile_calculation_end - profile_calculation_start)/CLOCKS_PER_SEC<<'\n';

	clock_t getHotPath_start = clock();
	printTopNodes(dag, 100);
	clock_t getHotPath_end = clock();
	errs()<<"Hotpath retrieval time: "<<(double)(getHotPath_end - getHotPath_start)/CLOCKS_PER_SEC<<'\n';
	outputPathProfile(dag);
}

class CurrTreeState {
	public:
		TreeNode* prn;
		TreeNode* pan;
		TreeNode* csn;
		CurrTreeState() : prn(NULL), pan(NULL), csn(NULL) {}
};

TreeNode* HPPTrace::buildSerializedDag(std::ifstream& fileHandle) {
	assert(fileHandle);

	std::stack<CurrTreeState> exploringStack;
	char eventId;

	uint32_t pathId;
	char pathIdBuf[PathIDSize];
	uint32_t funcId;
	char funcIdBuf[FuncIDSize];
	uint32_t untrackCallIndex;
	char indexBuf[UntrackCallIndexSize];
	uint32_t watchpointId;
	char watchpointIdBuf[WatchPointIDSize];
	int64_t pathState;
	char pathStateBuf[PathStateSize];

//	CurrTreeState trState;
//	trState.csn = new TreeNode(NT_IndeterCallSiteNode);
//	exploringStack.push(trState);
	TreeNode* threadNode = new TreeNode(NT_PhonyNode);
	while(fileHandle.read(&eventId, EventIDSize)){
		if (eventId == Events[ENTER_PROC]) {
			
			fileHandle.read(funcIdBuf, FuncIDSize);
			funcId = *((uint32_t*)funcIdBuf);

			CurrTreeState trState;
			trState.prn = new TreeNode(NT_ProcedureNode, funcId);
			trState.pan = new TreeNode();
			exploringStack.push(trState);
		} else if (eventId == Events[EXIT_PROC]) {
			// release the path node of current CurrTreeState
			CurrTreeState trState = exploringStack.top();
			delete trState.pan;
			exploringStack.pop();
			if (!exploringStack.empty()) {
				TreeNode* pred;
				if (exploringStack.top().csn) {
					pred = exploringStack.top().csn;
				} else {
					pred = exploringStack.top().pan;
				}
				pred->addChild(trState.prn);
			} else {
				threadNode->addChild(trState.prn);
			}
		} else if (eventId == Events[PATH]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			pathId = *((uint32_t*)pathIdBuf);

			CurrTreeState& topTrState = exploringStack.top();
			// update panString
			topTrState.pan->setBLPath(topTrState.prn->getFuncID(), pathId);

			topTrState.prn->addChild(topTrState.pan);
			topTrState.pan = new TreeNode();
		} else if (eventId == Events[ENTER_INDETER_CS]) {
			exploringStack.top().csn = new TreeNode(NT_IndeterCallSiteNode);
		} else if (eventId == Events[EXIT_INDETER_CS]) {
			CurrTreeState& topTrState = exploringStack.top();
			topTrState.pan->addChild(topTrState.csn);
			topTrState.csn = NULL;
		} else if (eventId == Events[ENTER_UNTRACK_CS]) {
			exploringStack.top().csn = new TreeNode(NT_UntrackCallSiteNode);
		} else if (eventId == Events[EXIT_UNTRACK_CS]) {
			fileHandle.read(indexBuf, UntrackCallIndexSize);
			untrackCallIndex = *((uint32_t*)indexBuf);
			
			CurrTreeState& topTrState = exploringStack.top();
			// set the index of the cs node as well as the node text
			topTrState.csn->setUntrackCallIndex(untrackCallIndex);

			topTrState.pan->addChild(topTrState.csn);
			topTrState.csn = NULL;
		} else if (eventId == Events[UNFINISHED_PATH]) {
			// unfinished path will not collide with previous subtree
			fileHandle.read(watchpointIdBuf, WatchPointIDSize);
			watchpointId = *((uint32_t*)watchpointIdBuf);
			fileHandle.read(pathStateBuf, PathStateSize);
			pathState = *((int64_t*)pathStateBuf);

			CurrTreeState trState = exploringStack.top();
			trState.pan->setUnfinishedPath(watchpointId, pathState);

			trState.prn->addChild(trState.pan);
			// handle the procedure call node
			exploringStack.pop();
			if ( !exploringStack.empty() ) {
				if (exploringStack.top().csn) {
					if (exploringStack.top().csn->getType() == NT_UntrackCallSiteNode) {
						//TODO get the untrackCallIndex on early termination
						exploringStack.top().csn->setUntrackCallIndex(0/*temporary value*/);
					}
					exploringStack.top().csn->addChild(trState.prn);
					// add csn as child node to the pan
					// when only the phony CurrWPPTreeState is on stack, do not add csn as a child since it has no pan
					if (exploringStack.size() != 1)
						exploringStack.top().pan->addChild(exploringStack.top().csn);
				} else {
					exploringStack.top().pan->addChild(trState.prn);
				}
			} else {
				threadNode->addChild(trState.prn);
			}
		} else {
			assert("ERROR: unidentified log content\n");
		}
	} 
//	CurrTreeState& topTrState = exploringStack.top();
//	uint32_t root = topTrState.csn->checkDuplication();
//	dictIndex.write((char*)(&root), sizeof(uint32_t));
//	delete topTrState.csn;
//	return root;
	return threadNode;
}

void HPPTrace::loadHPPTree(std::ifstream& fileHandle) {
	assert(fileHandle);

	char eventId;

	uint32_t pathId;
	char pathIdBuf[PathIDSize];
	uint32_t funcId;
	char funcIdBuf[FuncIDSize];
	uint32_t untrackCallIndex;
	char indexBuf[UntrackCallIndexSize];
	uint32_t watchpointId;
	char watchpointIdBuf[WatchPointIDSize];
	int64_t pathState;
	char pathStateBuf[PathStateSize];

	while(fileHandle.read(&eventId, EventIDSize)){
		if (eventId == Events[ENTER_PROC]) {
			fileHandle.read(funcIdBuf, FuncIDSize);
			funcId = *((uint32_t*)funcIdBuf);
		} else if (eventId == Events[EXIT_PROC]) {
			// release the path node of current CurrTreeState
		} else if (eventId == Events[PATH]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			pathId = *((uint32_t*)pathIdBuf);

		} else if (eventId == Events[ENTER_INDETER_CS]) {
		} else if (eventId == Events[EXIT_INDETER_CS]) {
		} else if (eventId == Events[ENTER_UNTRACK_CS]) {
		} else if (eventId == Events[EXIT_UNTRACK_CS]) {
			fileHandle.read(indexBuf, UntrackCallIndexSize);
			untrackCallIndex = *((uint32_t*)indexBuf);
		} else if (eventId == Events[UNFINISHED_PATH]) {
			// unfinished path will not collide with previous subtree
			fileHandle.read(watchpointIdBuf, WatchPointIDSize);
			watchpointId = *((uint32_t*)watchpointIdBuf);
			fileHandle.read(pathStateBuf, PathStateSize);
			pathState = *((int64_t*)pathStateBuf);
		} else {
			assert("ERROR: unidentified log content\n");
		}
	} 
}

enum ControlFlowState {
	TRACKING,
	IN_UNTRACK_CALL,
	HAS_UNTRACK_CALLEE
};

class CurrWPPTreeState : public CurrTreeState {
	public:
		std::vector<uint32_t> wppBLPaths;
		bool indeterCSFlag;
		ControlFlowState cfs;
		uint32_t untrackCallIndex;
		CurrWPPTreeState() : CurrTreeState(), indeterCSFlag(false), cfs(TRACKING), untrackCallIndex(0) {}
		
};

std::vector<uint32_t>* HPPTrace::generateBacktraceTestCases(unsigned repeats) {
	double RANGE = 0;
	// load the list of exercised procedure
	std::ifstream procList (ProcList.c_str(), std::ios::in | std::ios::binary);

	std::vector<ProcProfile> procProfiles;
	uint32_t procID;
	uint32_t procCount;
	while (procList.read((char*)&procID, sizeof(uint32_t))) {
		ProcProfile prof;
		prof.procID = procID;
		procList.read((char*)&procCount, sizeof(uint32_t));
		prof.count = procCount;
		procProfiles.push_back(prof);
	}
	// sort the procProfiles based on the count element in ascending order
	std::sort(procProfiles.begin(), procProfiles.end(), procProfileComp);

	int range = std::floor(procProfiles.size()*RANGE);
	int sizeInRange = procProfiles.size()-2*range;
	// randomly select "repeats" exercised procedures with replacement
	std::vector<uint32_t> *testCases = new std::vector<uint32_t>();
	// use the current hour as the seed 
//	srand(time(NULL)/(3600*24*7));
	srand(1418701483);
	unsigned backtraceTotal = 0;
	for (unsigned i=0; i<repeats; ++i) {
		unsigned randIndex = rand()%sizeInRange;
		ProcProfile pro = procProfiles[range+randIndex];
		testCases->push_back(pro.procID);
		backtraceTotal += pro.count;
//		outs()<<"test case "<<i<<": proc_"<<pro.procID<<", count "<<pro.count<<"\n";
	}
	errs()<<"Total Num of Backtraces: "<<backtraceTotal<<"\n";
	return testCases;
}

void HPPTrace::queryBLPTBackTrace(unsigned repeats) {
	std::vector<uint32_t> *testCases = generateBacktraceTestCases(repeats);
	clock_t query_start = clock();
	for (unsigned i=0; i<testCases->size(); ++i) {
		outs()<<"ProcID to be backtraced: "<<testCases->at(i)<<'\n';

		for (unsigned i = 0; i != RawTraceFiles.size(); ++i) {
			// processing each thread-local trace file
			unsigned traceNum = 0;
			while (true) {
				std::stringstream sstm;
				sstm << RawTraceFiles[i] << "/trace" << traceNum << ".log";
				std::ifstream logFile (sstm.str().c_str(), std::ios::in | std::ios::binary);
				if (logFile.is_open()) {
					printBackTraceFromRawTrace(logFile, testCases->at(i));
					traceNum++;
					logFile.close();
				} else {
					break;
				}
			}
		}
	}
	clock_t query_end = clock();
	// output the result
	errs()<<"Query time: "<<(double)(query_end - query_start)/CLOCKS_PER_SEC<<'\n';
}

void HPPTrace::printBackTraceFromRawTrace(std::ifstream& fileHandle, uint32_t tracedProcID) {
	struct TraceStackElement {
		uint32_t procID;
		std::vector<uint32_t> intraPaths;
	};
	assert(fileHandle);
	std::vector<TraceStackElement> exploringStack;

	char eventId;
	uint32_t pathId;
	char pathIdBuf[PathIDSize];
	uint32_t funcId;
	char funcIdBuf[FuncIDSize];

	while(fileHandle.read(&eventId, EventIDSize)){
		if (eventId == Events[ENTER_PROC]) {
			fileHandle.read(funcIdBuf, FuncIDSize);
			funcId = *((uint32_t*)funcIdBuf);

			if (funcId == tracedProcID) {
			}
			TraceStackElement tse;
			tse.procID = funcId;
			exploringStack.push_back(tse);
		} else if (eventId == Events[EXIT_PROC]) {
			exploringStack.pop_back();
		} else if (eventId == Events[PATH] || eventId == Events[PATH2TRACKCS] || eventId == Events[PATH2INDETERCS] || eventId == Events[PATH2UNTRACKCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			pathId = *((uint32_t*)pathIdBuf);
			exploringStack.back().intraPaths.push_back(pathId);
		} else {
			assert("ERROR: unidentified log content\n");
		}
	}
}

// premise: paths are a sequence of BL path id in wpp split by call sites that map to one single original BL path
uint32_t HPPTrace::retrieveOriginalBLPathID(uint32_t funcID, const std::vector<uint32_t>& paths) {
	uint32_t originalPathID = 0;
	// use funcID to locate the pathMappingHashtable of the corresponding function
	PathMappingHash& mappingHash = *(wppPathMappingHash[funcID]);
	for (unsigned i=0; i<paths.size(); ++i) {
		PathMappingHash::iterator it = mappingHash.find(paths[i]);
		if (it != mappingHash.end()) {
			originalPathID += it->second;
		} else {
			pathTracing::BLWPPRecoveryDag* dag = wppRecoveryDags[funcID];
			uint32_t mappedPathID = dag->getOriginalPathState(paths[i]);
			originalPathID += mappedPathID;
			mappingHash[paths[i]] = mappedPathID;
		}
	}
	return originalPathID;
}

void TreeNode::createNodeData() {
	NodeType nt = getType();
  char* it = NULL;
	if (nt == NT_ProcedureNode) {
    nodeDataSize = PROC_NODE_DATA_SIZE;
    nodeData = new char[nodeDataSize];
    it = nodeData;
    *((uint32_t*)it) = getFuncID();
    it += FuncIDSize;
	} else if (nt == NT_PathNode) {
    nodeDataSize = PATH_NODE_DATA_SIZE;
    nodeData = new char[nodeDataSize];
    it = nodeData;
    *((uint32_t*)it) = getPathID();
    it += PathIDSize;
  } else if (nt == NT_UnfinishedPathNode) {
    nodeDataSize = UPATH_NODE_DATA_SIZE;
    nodeData = new char[nodeDataSize];
    it = nodeData;
    *((uint32_t*)it) = getWatchPointID();
    it += WatchPointIDSize;
    *((int64_t*)it) = getPathState();
    it += PathStateSize;
  } else if (nt == NT_IndeterCallSiteNode) {
    nodeDataSize = INDETER_CS_NODE_DATA_SIZE;
    nodeData = new char[nodeDataSize];
    it = nodeData;
	} else {
    nodeDataSize = UNTRACK_CS_NODE_DATA_SIZE;
    nodeData = new char[nodeDataSize];
    it = nodeData;
    *((uint32_t*)it) = getUntrackCallIndex();
    it += UntrackCallIndexSize;
	}
  *((uint32_t*)it) = getChildrenNum();
}

// updated version
// prerequisite: the tree node is complete
uint32_t TreeNode::checkDuplication() {
	static uint32_t currSig = 0;
	NodeType nt = getType();

	// phony nodes do not need to be stored in hashtable for check of duplication
	if (nt == NT_PhonyNode) {
		uint32_t sig = currSig;
		++currSig;
		writeCompressedDagGrammar();
		return sig;
	}

  // TEST
	SigHash* pSigHash;
	if (nt == NT_ProcedureNode) {
		pSigHash = procHash;
	} else if (nt == NT_PathNode || nt == NT_UnfinishedPathNode) {
		pSigHash = pathHash;
	} else {
		pSigHash = callsiteHash;
	}
	std::string nodeText;
	constructNodeText(nodeText);
	if (!isNewNode()) {
		SigHash::iterator it = pSigHash->find(nodeText);
		if (it != pSigHash->end()) {
			// duplicated, return the reference to the existing node
			return it->second;
		} else {
			// new node 
			setIsNewNode();
		}
	}
	uint32_t sig = currSig;
	++currSig;
	// add a new signature and node reference pair (update the hash set)
	pSigHash->insert(std::make_pair(nodeText, sig));
	// write to file
	writeCompressedDagGrammar();
	return sig;
}

//--------------------------------------------------------------------------------------------------------//

const BasicBlockPath* HPPTrace::retrievePath(uint32_t functionID, uint32_t pathID) {
	BasicBlockPath* blockSeq;
	FunctionToPath::iterator fun_it = this->pathPoolMap.find(functionID);
	if(fun_it == pathPoolMap.end())
	{
		blockSeq = hppRecoveryDags[functionID]->getPathBB(pathID);
		this->pathPoolMap[functionID][pathID] = blockSeq;
	}
	else
	{
		PathToBB::iterator path_it = fun_it->second.find(pathID);
		if(path_it == fun_it->second.end())
		{
			blockSeq = hppRecoveryDags[functionID]->getPathBB(pathID);
			fun_it->second[pathID] = blockSeq;
		}
		else
		{
			blockSeq = path_it->second;
		}
	}
	return blockSeq;
}

HPPTrace::ConstBBPathInsnIterator::ConstBBPathInsnIterator(const BasicBlockPath& bbPath):path(bbPath) {
	this->nextBBIter = path.cbegin();
	this->bbEnd = path.cend();
	if (this->nextBBIter != this->bbEnd) {
		const BasicBlock* blk = *(this->nextBBIter);
		this->nextInsnIter = blk->begin();
		this->insnEnd = blk->end();
		++this->nextBBIter;
	} else {
		this->nextInsnIter = this->insnEnd;
	}
}

const Instruction* HPPTrace::ConstBBPathInsnIterator::getNextInsn() {
	if (this->nextInsnIter == this->insnEnd) {
		if (this->nextBBIter == this->bbEnd) {
			return NULL;
		} else {
			const BasicBlock* blk = *(this->nextBBIter);
			this->nextInsnIter = blk->begin();
			this->insnEnd = blk->end();
			++this->nextBBIter;
		}
	}
	const Instruction* insn = this->nextInsnIter;	
	++this->nextInsnIter;
	return insn;
}

void HPPTrace::printTrace(std::ifstream& fileHandle) {
	assert(fileHandle);
	char eventId;

	uint32_t pathId;
	char pathIdBuf[PathIDSize];
	uint32_t funcId;
	char funcIdBuf[FuncIDSize];
	uint32_t untrackCallIndex;
	char indexBuf[UntrackCallIndexSize];
	uint32_t watchpointId;
	char watchpointIdBuf[WatchPointIDSize];
	int64_t pathState;
	char pathStateBuf[PathStateSize];

	while(fileHandle.read(&eventId, EventIDSize)){
		if (eventId == Events[ENTER_PROC]) {
			fileHandle.read(funcIdBuf, FuncIDSize);
			if (fileHandle.gcount() != FuncIDSize) {
				errs()<<"ERROR: fail reading funcID\n";
				return;
			}
			funcId = *((uint32_t*)funcIdBuf);
			outs()<<"EnterFunc: "<<funcId<<"\n";

		} else if (eventId == Events[EXIT_PROC]) {
			outs()<<"ExitFunc\n";
		} else if (eventId == Events[PATH]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			pathId = *((uint32_t*)pathIdBuf);
			outs()<<"	Path: "<<pathId<<"\n";
		} else if (eventId == Events[PATH2TRACKCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			pathId = *((uint32_t*)pathIdBuf);
			outs()<<"	Path to track cs: "<<pathId<<"\n";
		} else if (eventId == Events[PATH2INDETERCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			pathId = *((uint32_t*)pathIdBuf);
			outs()<<"	Path to indeter cs: "<<pathId<<"\n";
		} else if (eventId == Events[PATH2UNTRACKCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			pathId = *((uint32_t*)pathIdBuf);
			outs()<<"	Path to untrack cs: "<<pathId<<"\n";
		} else if (eventId == Events[ENTER_INDETER_CS]) {
			outs()<<"~Pre Indeter\n";
		} else if (eventId == Events[EXIT_INDETER_CS]) {
			outs()<<"~Post Indeter\n";
		} else if (eventId == Events[ENTER_UNTRACK_CS]) {
			outs()<<"~Pre Untrack\n";
		} else if (eventId == Events[EXIT_UNTRACK_CS]) {
			fileHandle.read(indexBuf, UntrackCallIndexSize);
			if (fileHandle.gcount() != UntrackCallIndexSize) {
				errs()<<"ERROR: fail reading untrack call index\n";
				return;
			}
			untrackCallIndex = *((uint32_t*)indexBuf);
			outs()<<"~Post Untrack: "<<untrackCallIndex<<"\n";
			
		} else if (eventId == Events[UNFINISHED_PATH]) {
			fileHandle.read(watchpointIdBuf, WatchPointIDSize);
			if (fileHandle.gcount() != WatchPointIDSize) {
				errs()<<"ERROR: fail reading watch point ID\n";
				return;
			}
			watchpointId = *((uint32_t*)watchpointIdBuf);
			fileHandle.read(pathStateBuf, PathStateSize);
			if (fileHandle.gcount() != PathStateSize) {
				errs()<<"ERROR: fail reading path state\n";
				return;
			}
			pathState = *((int64_t*)pathStateBuf);

			outs()<<"WatchPoint: "<<watchpointId<<"\n";
			outs()<<"pathState: "<<pathState<<"\n";
		} else {
			errs()<<"ERROR: unidentified log content\n";
			return;
		}
	} 
}

void HPPTrace::collectStatistics(std::ifstream& fileHandle) {
	assert(fileHandle);
	char eventId;

	char pathIdBuf[PathIDSize];
	char funcIdBuf[FuncIDSize];
	char indexBuf[UntrackCallIndexSize];
	char watchpointIdBuf[WatchPointIDSize];
	char pathStateBuf[PathStateSize];

	while(fileHandle.read(&eventId, EventIDSize)){
		if (eventId == Events[ENTER_PROC]) {
			fileHandle.read(funcIdBuf, FuncIDSize);
			if (fileHandle.gcount() != FuncIDSize) {
				errs()<<"ERROR: fail reading funcID\n";
				return;
			}
			++enterProcCounter;
		} else if (eventId == Events[EXIT_PROC]) {
			++exitProcCounter;
		} else if (eventId == Events[PATH]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			++pathCounter;
		} else if (eventId == Events[PATH2TRACKCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			++path2TrackCSCounter;
		} else if (eventId == Events[PATH2INDETERCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			++path2IndeterCSCounter;
		} else if (eventId == Events[PATH2UNTRACKCS]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			++path2UntrackCSCounter;
		} else if (eventId == Events[ENTER_INDETER_CS]) {
			++enterIndeterCSCounter;
		} else if (eventId == Events[EXIT_INDETER_CS]) {
			++exitIndeterCSCounter;
		} else if (eventId == Events[ENTER_UNTRACK_CS]) {
			++enterUntrackCSCounter;
		} else if (eventId == Events[EXIT_UNTRACK_CS]) {
			fileHandle.read(indexBuf, UntrackCallIndexSize);
			if (fileHandle.gcount() != UntrackCallIndexSize) {
				errs()<<"ERROR: fail reading untrack call index\n";
				return;
			}
			++exitUntrackCSCounter;
		} else if (eventId == Events[UNFINISHED_PATH]) {
			fileHandle.read(watchpointIdBuf, WatchPointIDSize);
			if (fileHandle.gcount() != WatchPointIDSize) {
				errs()<<"ERROR: fail reading watch point ID\n";
				return;
			}
			fileHandle.read(pathStateBuf, PathStateSize);
			if (fileHandle.gcount() != PathStateSize) {
				errs()<<"ERROR: fail reading path state\n";
				return;
			}
			++unfinishedPathCounter;
		} else {
			errs()<<"ERROR: unidentified log content\n";
			return;
		}
	} 
}

uint32_t HPPNode::getFuncID() const {
	assert(Type == NT_ProcedureNode);
	ProcedureLabel* label = (ProcedureLabel*)nodeLabel;
	return label->getFuncID();
}

uint32_t HPPNode::getPathID() const {
	assert(Type == NT_PathNode);
	BLPathLabel* label = (BLPathLabel*)nodeLabel;
	return label->getPathID();
}

uint32_t HPPNode::getUntrackCallIndex() const {
	assert(Type == NT_UntrackCallSiteNode);
	UntrackCallSiteLabel* label = (UntrackCallSiteLabel*)nodeLabel;
	return label->getUntrackCallIndex();
}

uint32_t HPPNode::getWatchPointID() const {
	assert(Type == NT_UnfinishedPathNode);
	UnfinishedPathLabel* label = (UnfinishedPathLabel*)nodeLabel;
	return label->getWatchPointID();
}

int64_t HPPNode::getPathState() const {
	assert(Type == NT_UnfinishedPathNode);
	UnfinishedPathLabel* label = (UnfinishedPathLabel*)nodeLabel;
	return label->getPathState();
}

// updated version, for dictionary generation use
void TreeNode::addChild(TreeNode* child) {
	// check duplication and return the child's signature
	// update the children list
	uint32_t childSig = child->checkDuplication();
	if (!childrenSig.empty() && childrenSig.back().signature == childSig) {
		// repeat is found
		++(childrenSig.back().numOfRepeats);
	} else {
		// append new child
		NodeSignature nSig;
		nSig.numOfRepeats = 1;
		nSig.signature = childSig;
		childrenSig.push_back(nSig);
	}
//  static uint32_t count = 0;
//  ++count;
//  llvm::outs() << "Node-" << count << ": ";
	// update its isNew state
	if (child->isNewNode()) {
		setIsNewNode();
//    llvm::outs() << "new node detected\n";
	} //TEST
  //else {
	// remove the child tree node
 //   llvm::outs() << "repeated node detected\n";
	delete child;
  //}
}

TreeNode::TreeNode(NodeType T, uint32_t funcId): /*signature(0),*/ isNew(false), parentFuncID(0) {
	setType(T);
	Label* procLabel = new ProcedureLabel(funcId);
	setLabel(procLabel);
}

void TreeNode::setBLPath(uint32_t funcId, uint32_t pathId) {
	setType(NT_PathNode);
	Label* blPathLabel = new BLPathLabel(pathId);
	setLabel(blPathLabel);
	parentFuncID = funcId;
}

void TreeNode::setUntrackCallIndex(uint32_t index) {
	Label* indexLabel = new UntrackCallSiteLabel(index);
	setLabel(indexLabel);
}

void TreeNode::setUnfinishedPath(uint32_t watchpointId, int64_t pathState) {
	setType(NT_UnfinishedPathNode);
	Label* unfinishedPathLabel = new UnfinishedPathLabel(watchpointId, pathState);
	setLabel(unfinishedPathLabel);
}

void TreeNode::constructNodeText(std::string& str) {
	// get node's children sig list as array
	char* childrenSig = (char*)(getChildrenSig().data());
	size_t childrenSigSize = sizeof(NodeSignature) * getChildrenSig().size();
	char* nodeTextArray;
	size_t textArraySize;
	switch (getType()) {
		case NT_ProcedureNode:
			{
				// nodeText <ProcID, childrenSig>
				textArraySize = childrenSigSize + FuncIDSize;
				nodeTextArray = new char[textArraySize];

				uint32_t funcID = getFuncID();

				memcpy(nodeTextArray, (char*)&funcID, FuncIDSize);
				memcpy(nodeTextArray+FuncIDSize, childrenSig, childrenSigSize);

				break;
			}
		case NT_PathNode:
			{
				// nodeText <PathID, childrenSig>
				textArraySize = childrenSigSize + PathIDSize;
				nodeTextArray = new char[textArraySize];

				uint32_t pathID = getPathID();

				memcpy(nodeTextArray, (char*)&pathID, PathIDSize);
				memcpy(nodeTextArray+PathIDSize, childrenSig, childrenSigSize);

				break;
			}
		case NT_IndeterCallSiteNode:
			{
				// nodeText <childrenSig>
				textArraySize = childrenSigSize;
				nodeTextArray = new char[textArraySize];
				memcpy(nodeTextArray, childrenSig, childrenSigSize);
				break;
			}
		case NT_UntrackCallSiteNode:
			{
				// nodeText <callsite index, childrenSig>
				textArraySize = childrenSigSize + UntrackCallIndexSize;
				nodeTextArray = new char[textArraySize];

				uint32_t index = getUntrackCallIndex();

				memcpy(nodeTextArray, (char*)&index, UntrackCallIndexSize);
				memcpy(nodeTextArray+UntrackCallIndexSize, childrenSig, childrenSigSize);

				break;
			}
		case NT_UnfinishedPathNode:
			{
				// nodeText <watchpointID, PathState, childrenSig>
				textArraySize = childrenSigSize + WatchPointIDSize + PathStateSize;
				nodeTextArray = new char[textArraySize];

				uint32_t watchPointID = getWatchPointID();
				int64_t pathState = getPathState();

				memcpy(nodeTextArray, (char*)&watchPointID, WatchPointIDSize);
				memcpy(nodeTextArray+WatchPointIDSize, (char*)&pathState, PathStateSize);
				memcpy(nodeTextArray+WatchPointIDSize+PathStateSize, childrenSig, childrenSigSize);

				break;
			}
		default:
			assert("unidentified node type");
	}
	str = std::string(nodeTextArray, textArraySize);
	delete[] nodeTextArray;
}

// write tree gramar to file when new subtree is identified
// 	1. root node must have a new subtree
// 	2. in checkDuplication() when new subtree is identified
// 	3. Unfinished path node and its parent procedure node must lead new subtrees
// prerequisite:
// 	the tree node is complete, i.e., all its children have been completed && its
// 	signature is settled && its own label is settled
void TreeNode::writeCompressedDagGrammar() {
	char* bytes;
	size_t byteArraySize;
	
	char* childrenSig = (char*)(getChildrenSig().data());
	size_t childrenSigSize = sizeof(NodeSignature) * getChildrenSig().size();
	uint32_t numOfChildren = getChildrenSig().size();
	NodeType nt = getType();

	// <NodeType, NodeContent, ChildrenSignatures>
	// initialize the byteArray
	byteArraySize = NodeTypeSize + NumOfChildrenSize + childrenSigSize;
	switch (nt) {
		case NT_ProcedureNode:
			{
				byteArraySize += FuncIDSize;
				break;
			}
		case NT_PathNode:
			{
				byteArraySize += PathIDSize;
				break;
			}
		case NT_UntrackCallSiteNode:
			{
				byteArraySize += UntrackCallIndexSize;
				break;
			}
		case NT_IndeterCallSiteNode:
			{
				break;
			}
		case NT_UnfinishedPathNode:
			{
				byteArraySize += WatchPointIDSize + PathStateSize;
				break;
			}
		case NT_PhonyNode:
			{
				break;
			}
		default:
			assert("unidentified node type");
	}
	bytes = new char[byteArraySize];
	char* writeLoc = bytes;
	// store the NodeType
	memcpy(writeLoc, &Nodes[nt], NodeTypeSize);
	writeLoc += NodeTypeSize;
	// store the node content
	switch (nt) {
		case NT_ProcedureNode:
			{
				uint32_t funcID = getFuncID();
				memcpy(writeLoc, (char*)&funcID, FuncIDSize);
				writeLoc += FuncIDSize;
				break;
			}
		case NT_PathNode:
			{
				uint32_t pathID = getPathID();
				memcpy(writeLoc, (char*)&pathID, PathIDSize);
				writeLoc += PathIDSize;
				break;
			}
		case NT_UntrackCallSiteNode:
			{
				uint32_t index = getUntrackCallIndex();
				memcpy(writeLoc, (char*)&index, UntrackCallIndexSize);
				writeLoc += UntrackCallIndexSize;
				break;
			}
		case NT_IndeterCallSiteNode:
			{
				break;
			}
		case NT_UnfinishedPathNode:
			{
				uint32_t watchPointID = getWatchPointID();
				int64_t pathState = getPathState();

				memcpy(writeLoc, (char*)&watchPointID, sizeof(uint32_t));
				writeLoc += sizeof(uint32_t);

				memcpy(writeLoc, (char*)&pathState, sizeof(int64_t));
				writeLoc += sizeof(uint64_t);
				break;
			}
		case NT_PhonyNode:
			{
				break;
			}
		default:
			assert("unidentified node type");
	}
	// store the children
	memcpy(writeLoc, (char*)&numOfChildren, NumOfChildrenSize);
	writeLoc += NumOfChildrenSize;
	memcpy(writeLoc, childrenSig, childrenSigSize);
	
	traceDict.write(bytes, byteArraySize);
	delete bytes;
}

void DagNode::printNode() {
	NodeType nt = this->getType();
	if (nt == NT_ProcedureNode) {
		outs()<<"(Proc:"<<this->getFuncID()<<")";
	} else if (nt == NT_PathNode) {
		outs()<<"(Path:"<<this->getPathID()<<")";
	} else if (nt == NT_UnfinishedPathNode) {
		outs()<<"(UPath:"<<this->getWatchPointID()<<","<<this->getPathState()<<")";
	} else if (nt == NT_UntrackCallSiteNode) {
		outs()<<"(UntrackCS:"<<this->getUntrackCallIndex()<<")";
	} else if (nt == NT_IndeterCallSiteNode) {
		outs()<<"(IndeterCS)";
	}
}

std::string DagNode::toString() {
	std::stringstream ss;
	NodeType nt = this->getType();
	if (nt == NT_ProcedureNode) {
		ss<<"(Proc:"<<this->getFuncID()<<")";
	} else if (nt == NT_PathNode) {
		ss<<"(Path:"<<this->getPathID()<<")";
	} else if (nt == NT_UnfinishedPathNode) {
		ss<<"(UPath:"<<this->getWatchPointID()<<","<<this->getPathState()<<")";
	} else if (nt == NT_UntrackCallSiteNode) {
		ss<<"(UntrackCS:"<<this->getUntrackCallIndex()<<")";
	} else if (nt == NT_IndeterCallSiteNode) {
		ss<<"(IndeterCS)";
	}
	return ss.str();
}

void HPPDag::printBackTraces(uint32_t tracedProcID) {
	std::vector<DagNode*> targetNodes;
	for (unsigned i=0; i<procNodes.size(); ++i) {
		if (procNodes[i]->getFuncID() == tracedProcID) {
			targetNodes.push_back(procNodes[i]);
		}
	}
	outs()<<"num of targetNodes: "<<targetNodes.size()<<'\n';

	std::vector<HPPEdge> pathStack;

	// for each target node, get its backtrace by reversed dfs to entryNode
	for (unsigned i=0; i<targetNodes.size(); ++i) {
		std::vector<HPPEdge>& preds = targetNodes[i]->getPreds();
		for (unsigned i=0; i<preds.size(); ++i) {
			pathStack.push_back(preds[i]);
		}

		while (!pathStack.empty()) {
			DagNode* node2Process = pathStack.back().targetNode;
			if (node2Process->getVisitState() == DagNode::VisitState::VISITED) {
				pathStack.pop_back();
				node2Process->setVisitState(DagNode::VisitState::UNVISITED);
				continue;
			}
			node2Process->setVisitState(DagNode::VisitState::VISITED);
			if (node2Process == rootNode) {
				continue;
			}
			std::vector<HPPEdge>& preds = node2Process->getPreds();
			for (unsigned i=0; i<preds.size(); ++i) {
				pathStack.push_back(preds[i]);
			}
		}
	}
}

void HPPDag::calculateHeight() {
	// initialize the indegree for all nodes
	for (unsigned i=0; i<procNodes.size(); ++i) {
		DagNode* node = procNodes[i];
		node->setIndegree(node->getSuccs().size());	
	}
	for (unsigned i=0; i<pathNodes.size(); ++i) {
		DagNode* node = pathNodes[i];
		node->setIndegree(node->getSuccs().size());	
	}
	for (unsigned i=0; i<callsiteNodes.size(); ++i) {
		DagNode* node = callsiteNodes[i];
		node->setIndegree(node->getSuccs().size());	
	}
	// initialize the stack with all entry nodes
	// all entry nodes should be call site nodes with incoming edge
	std::stack<DagNode*> zeroIndegreeNodes;
	for (unsigned i=0; i<leafNodes.size(); ++i) {
		DagNode* node = leafNodes[i];
		zeroIndegreeNodes.push(node);
		node->setHeight(0);
	}
	while (!zeroIndegreeNodes.empty()) {
		DagNode* node = zeroIndegreeNodes.top();
		zeroIndegreeNodes.pop();
		for (unsigned i=0; i<node->getPreds().size(); ++i) {
			// for each successor edge
			HPPEdge e = (node->getPreds())[i];
			DagNode* parent = e.targetNode;
			// update the parent node's height if needed
			if (parent->getHeight() < node->getHeight() + 1) {
				parent->setHeight(node->getHeight() + 1);
			}
			// update the parent node's indegree
			parent->setIndegree(parent->getIndegree() - 1);
			if (parent->getIndegree() == 0) {
				zeroIndegreeNodes.push(parent);
			}
		}
	}
}

void HPPDag::calculateProfiles() {
	// initialize the indegree for all nodes
	for (unsigned i=0; i<procNodes.size(); ++i) {
		DagNode* node = procNodes[i];
		node->setIndegree(node->getPreds().size());	
	}
	for (unsigned i=0; i<pathNodes.size(); ++i) {
		DagNode* node = pathNodes[i];
		node->setIndegree(node->getPreds().size());	
	}
	for (unsigned i=0; i<callsiteNodes.size(); ++i) {
		DagNode* node = callsiteNodes[i];
		node->setIndegree(node->getPreds().size());	
	}
	// push all thread nodes as zero indegree nodes
	for (unsigned i=0; i<rootNode->getSuccs().size(); ++i) {
		HPPEdge e = (rootNode->getSuccs())[i];
		DagNode* executionNode = e.targetNode;
		executionNode->setIndegree(1);
		for (unsigned j=0; j<executionNode->getSuccs().size(); ++j) {
			e = (executionNode->getSuccs())[j];
			e.targetNode->setIndegree(1);
		}
	}
	// initialize the stack with all entry nodes
	// all entry nodes should be call site nodes with incoming edge
	std::stack<DagNode*> zeroIndegreeNodes;
	zeroIndegreeNodes.push(rootNode);
	rootNode->setCount(1);
	while (!zeroIndegreeNodes.empty()) {
		DagNode* node = zeroIndegreeNodes.top();
		zeroIndegreeNodes.pop();
		for (unsigned i=0; i<node->getSuccs().size(); ++i) {
			// for each successor edge
			HPPEdge e = (node->getSuccs())[i];
			DagNode* child = e.targetNode;
			// update the child node's count
			child->setCount(child->getCount() + node->getCount()*e.numOfRepeats);
			// update the child node's indegree
			child->setIndegree(child->getIndegree() - 1);
			if (child->getIndegree() == 0) {
				zeroIndegreeNodes.push(child);
			}
		}
	}
}

void HPPTrace::getBenchInfo(Module& M) {
	unsigned monitoredProcCount = 0;
	unsigned untrackProcCount = 0;
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration()) {
			if (!F->isIntrinsic()) {
				++untrackProcCount;
			}
		} else {
			++monitoredProcCount;
		}
	}
	errs()<<"Num of monitored procedures: "<<monitoredProcCount<<'\n';
	errs()<<"Num of untrack procedures: "<<untrackProcCount<<'\n';

	unsigned monitoredCSCount = 0;
	unsigned indeterCSCount = 0;
	unsigned untrackCSCount = 0;
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration())
			continue;
		for (llvm::Function::iterator func_it = F->begin(), func_end = F->end(); func_it != func_end; ++func_it) {
			llvm::BasicBlock* blk = &*func_it;

			for (BasicBlock::iterator bb_it = blk->begin(), bb_end = blk->end(); bb_it != bb_end; ++bb_it) {
				BasicBlock::iterator insertPoint;
				if (CallInst* callInst = dyn_cast<CallInst>(&*bb_it)) {
					Function* calledFunc = callInst->getCalledFunction();
					if (!calledFunc) {
						++indeterCSCount;
					}else if (calledFunc->isDeclaration()) {
						++untrackCSCount;
					} else {
						++monitoredCSCount;
					}
				} else if (InvokeInst* invokeInst = dyn_cast<InvokeInst>(&*bb_it)) {
					Function* calledFunc = invokeInst->getCalledFunction();
					insertPoint = invokeInst;
					if (!calledFunc) {
						++indeterCSCount;
					} else if (calledFunc->isDeclaration()) {
						++untrackCSCount;
					} else {
						++monitoredCSCount;
					}
				}
			}
		}
	}
	errs()<<"Num of monitored call site: "<<monitoredCSCount<<'\n';
	errs()<<"Num of indeterministic call site: "<<indeterCSCount<<'\n';
	errs()<<"Num of untracked call site: "<<untrackCSCount<<'\n';

	uint64_t totalBLPaths = 0;
	// temporary
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration())
			continue;
		pathTracing::BLWPPRecoveryDag* dag = pathTracing::generateWPPPathNumberingDag(F);
		uint32_t numOfPaths = dag->getNumberOfPaths();
		totalBLPaths += numOfPaths;
	}
	errs()<<"Total Num of BL paths: "<<totalBLPaths<<'\n';
}
