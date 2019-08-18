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

#include "../include/BLInstrumentation.h"
#include "../include/LogInterface.h"

using namespace llvm;

typedef std::vector<const BasicBlock*> BasicBlockPath;

namespace {

class WPPTrace : public ModulePass {
	public:
		static char ID; // Pass identification, replacement for typeid
//		WPPTrace() : ModulePass(ID), currPathSig(0), currProCSig(0), dags(), pathPool(NULL),
//			enterProcCounter(0), exitProcCounter(0), blPathCounter(0),
//			enterIndCSCounter(0), exitIndCSCounter(0), uPathCounter(0),
//			callPathCounter(0), nonCallPathCounter(0), unfindPathCounter(0) {}
		WPPTrace() : ModulePass(ID), dags(), pathPool(NULL),
			enterProcCounter(0), exitProcCounter(0), blPathCounter(0) {}
	private:
//		LLVMContext* context;
		virtual bool runOnModule(Module &M);
		std::vector<pathTracing::BLDag*> dags;
		// Two-dimensional array of type vector<llvm::BasicBlock*>*
		//	the first dimension is the function ID
		//	the second dimension is the path ID
		//	The array element is a pointer pointing to a sequence(vector) of BasicBlock*
		//		that the corresponding path traverses
		BasicBlockPath*** pathPool;
//		void init(Module& M);
//		void printTrace(std::ifstream& fileHandle);
		void collectStatistics(std::ifstream& fileHandle);
		const BasicBlockPath* retrievePath(uint32_t functionID, uint32_t pathID);

		// statistics
		uint64_t enterProcCounter;
		uint64_t exitProcCounter;
		uint64_t blPathCounter;
};
} // end anonymous namespace

#endif
