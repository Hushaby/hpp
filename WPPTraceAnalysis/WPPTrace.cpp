#include "WPPTrace.h"
#include <utility>
#include "llvm/Support/raw_ostream.h"
#include "../include/BLInstrumentation.h"
//using namespace HPPDag;
char WPPTrace::ID = 0;
static RegisterPass<WPPTrace> X("WPPTrace", "WPP Trace Analysis");
cl::opt<std::string> TraceFilePath("trace-path", cl::desc("Specify the trace file path"),
		cl::value_desc("trace file path"));

cl::opt<bool> ShowStat("show-statistics", cl::desc("show the statistics of the trace log"), cl::init(false));

bool WPPTrace::runOnModule(Module &M) {

	DEBUG(dbgs()
        << "********************************************\n"
        << "********************************************\n"
        << "**                                        **\n"
        << "**          WPP Trace Analysis            **\n"
        << "**                                        **\n"
        << "********************************************\n"
        << "********************************************\n");

//	context = &(M.getContext());
	Function *Main = M.getFunction("main");

	if (!Main) {
		errs() << "WARNING: no main function is found!\n";
		return false;
	}
	uint32_t functionID = 0;
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration())
			continue;
		outs()<<functionID<<": "<<F->getName()<<'\n';

			pathTracing::BLDag* dag = pathTracing::generateHPPPathNumberingDag(F);
//			dag->generateDotGraph(true);
			outs()<<"Num of hpp paths "<<dag->getNumberOfPaths()<<"\n";
			outs()<<"Num of hpp nodes: "<<dag->getNodesNum()<<"\n";
			outs()<<"Num of hpp edges: "<<dag->getEdgesNum()<<"\n\n";

//			pathTracing::BLWPPRecoveryDag* blptDag = pathTracing::generateWPPPathNumberingDag(F);
			pathTracing::BLDag* blptDag = pathTracing::generateWPPDag(F);
//			blptDag->generateDotGraph(true);
			outs()<<"Num of blpt paths "<<blptDag->getNumberOfPaths()<<"\n";
			outs()<<"Num of blpt nodes: "<<blptDag->getNodesNum()<<"\n";
			outs()<<"Num of blpt edges: "<<blptDag->getEdgesNum()<<"\n";
//			outs()<<"Num of blpt original paths "<<((pathTracing::BLWPPRecoveryNode*)blptDag->getRoot())->getNumOfOriginalPaths()<<"\n";
//		}
		++functionID;
	}
	return false;
	// read in the trace files
	int traceNum = 0;
	while (true) {
		std::stringstream sstm;
		std::string logFilePrefix = TraceFilePath + "/trace";// + traceNum + ".log";
		sstm << logFilePrefix << traceNum << ".log";
		std::ifstream logFile (sstm.str().c_str(), std::ios::in | std::ios::binary);
		if (logFile.is_open()) {
//			outs()<<sstm.str()<<" is on process\n";

			if (ShowStat) {
				outs()<<"Collect statistics on "<<sstm.str()<<'\n';
				collectStatistics(logFile);
			}
			traceNum++;
			logFile.close();
		} else {
			outs()<<"Finish processing all log files\nNum of thread processed: "<<traceNum<<'\n';
			break;
		}
	}
	if (ShowStat) {
		//output the statistics
		outs()<<"statistics:\n";
		outs()<<"Num of ENTER_PROC: "<<enterProcCounter<<'\n';
		outs()<<"Num of EXIT_PROC: "<<exitProcCounter<<'\n';
		outs()<<"Num of PATH: "<<blPathCounter<<'\n';
	}
	return false;
}


const BasicBlockPath* WPPTrace::retrievePath(uint32_t functionID, uint32_t pathID) {

	BasicBlockPath* blockSeq = this->pathPool[functionID][pathID];
	if (!blockSeq) {
		blockSeq = dags[functionID]->getPathBB(pathID);
		this->pathPool[functionID][pathID] = blockSeq;
	}
	return blockSeq;
}

void WPPTrace::collectStatistics(std::ifstream& fileHandle) {
	assert(fileHandle);
	char eventId;

	uint32_t pathId;
	char pathIdBuf[PathIDSize];
	uint32_t funcId;
	char funcIdBuf[FuncIDSize];

//	std::stack<int> procStack;
	while(fileHandle.read(&eventId, EventIDSize)){
		if (eventId == Events[ENTER_PROC]) {
			fileHandle.read(funcIdBuf, FuncIDSize);
			if (fileHandle.gcount() != FuncIDSize) {
				errs()<<"ERROR: fail reading funcID\n";
				return;
			}
			funcId = *((uint32_t*)funcIdBuf);
//			procStack.push(funcId);
			++enterProcCounter;
		} else if (eventId == Events[EXIT_PROC]) {
//			procStack.pop();
			++exitProcCounter;
		} else if (eventId == Events[PATH]) {
			fileHandle.read(pathIdBuf, PathIDSize);
			if (fileHandle.gcount() != PathIDSize) {
				errs()<<"ERROR: fail reading pathID\n";
				return;
			}
			pathId = *((uint32_t*)pathIdBuf);

			++blPathCounter;
		}
	} 
}
