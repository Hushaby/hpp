#include <llvm/ADT/SmallVector.h>
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <cstdint>

#include <unordered_set>
#include "../include/BLInstrumentation.h"
#include "TracingRuntime/tracing_runtime_interface.h"

using namespace llvm;

namespace {

class HPPTransform : public ModulePass {
	public:
		static char ID; // Pass identification, replacement for typeid
		HPPTransform() : ModulePass(ID){}
	private:
		LLVMContext* context;
		// the set of procedures which will not be treated as indeterministic call site
		// in default the set contains all probes functions inserted by HPP
		std::unordered_set<std::string> exempt_procedure_list;

//		Constant* initMainThreadFunction;
		Constant* HPPFuncEntryFunction;
		Constant* HPPFuncExitFunction;
		Constant* HPPExceptHandlingFunction;
		Constant* HPPForkHandlingFunction;
		Constant* HPPCleanupFunction;
		Constant* atExitFunction;
		Constant* HPPPathFunction;

		// For indeterministic call site instrumentation (function pointers)
		Constant* HPPPreIndCallFunction;
		Constant* HPPPostIndCallFunction;

		// For untracked call site
		Constant* HPPPreUntrackCallFunction;
		Constant* HPPPostUntrackCallFunction;

		// For pathState event instrumentation
		Constant* HPPWatchPointFunction;

		virtual bool runOnModule(Module &M);
		// the last parameter 'bool noWatchPoint' is used to test the performance of turning off watch point
		void declareProbes(Module& M, bool noWatchPoint);
		void instrument_cleanup(Function* main);
		void instrument_enter_procedure(Function* func, uint32_t functionID);
		void instrument_exception_handler(Function* func, uint32_t functionID);
		void instrument_exit_procedure(Function* func);
		void instrument_indeter_call_site(Function* func);
		void instrument_fork_handler(Function* func);
		void instrument_enter_indeter_cs(BasicBlock::iterator insertPoint);
		void instrument_exit_indeter_cs(BasicBlock::iterator insertPoint/*, uint16_t indeterCallSiteID*/);
		void instrument_enter_untrack_cs(BasicBlock::iterator insertPoint);
		void instrument_exit_untrack_cs(BasicBlock::iterator insertPoint);
		void instrument_watch_point(pathTracing::BLDag* dag);
//		void instrument_watch_point(Function* func, BLDag* dag);
};

} // end anonymous namespace


//std::vector<FunctionShadow> functionShadows = std::vector<FunctionShadow>();
//__gnu_cxx::hash_set<std::string> ind_call_set = __gnu_cxx::hash_set<std::string>();

char HPPTransform::ID = 0;
static RegisterPass<HPPTransform> X("HPPTransform", "Transformation Pass for Bitcode instrumentation of HPP");
cl::opt<bool> WatchPointOff("watchpoint-off", cl::desc("turn off the watch point"), cl::init(false));

bool HPPTransform::runOnModule(Module &M) {

	DEBUG(dbgs()
        << "********************************************\n"
        << "********************************************\n"
        << "**                                        **\n"
        << "**          HPP INSTRUMENTATION           **\n"
        << "**                                        **\n"
        << "********************************************\n"
        << "********************************************\n");

	context = &(M.getContext());
	declareProbes(M, WatchPointOff);
	// No main, no instrumentation!
	Function *Main = M.getFunction("main");

	if (!Main) {
		errs() << "WARNING: cannot insert path profiling into a module"
			<< " with no main function!\n";
		return false;
	}
	// instrument tracked procedures
	uint32_t functionID = 0;
	for (Module::iterator F = M.begin(), E = M.end(); F != E; F++) {
		// the default set of tracked procedures are all bitcode-available procedures
		if (F->isDeclaration())
			continue;
		DEBUG(dbgs() << "Function: " << F->getName() << "\n");

		instrument_enter_procedure(F, functionID);
		instrument_exception_handler(F, functionID);
		
		pathTracing::BLDag* dag = pathTracing::generateHPPPathInstrumentationDag(F);
		pathTracing::setNormalBLPathProbe(this->HPPPathFunction);
		pathTracing::placeBLPathInstrumentation(dag, this->context, this);
		// Note: instrument_indeter_call_site must be instrumented after placeBLPathInstrumentation
		instrument_indeter_call_site(F);
		instrument_fork_handler(F);
		instrument_watch_point(dag);
		// Note: procedure exit probes must be instrumented last in order to avoid the order
		// issues with other probes
		instrument_exit_procedure(F);

		++functionID;
	}
	// instrument cleanup function to finish writing to the trace file 
	// NOTE: this has to be instrumented after the other probes
	// because there may also be user-defined atexit procedures
	// which need to be tracked while the one instrumented by HPP will not
	//		instrument_cleanup(Main);
	return true;
}


void HPPTransform::declareProbes(Module& M, bool noWatchPoint) {
	std::string probeName;

	probeName = noWatchPoint? HppFuncEntryProcNameWithoutWatchPoint: HppFuncEntryProcName;
	this->HPPFuncEntryFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)), //return void
			Type::getInt32Ty(*(this->context)), //parameter unsigned functionID
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// c++_updated
	// probe used by instrument_exception_handler
	probeName = HppExceptHandlingProcName;
	this->HPPExceptHandlingFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt32Ty(*(this->context)), //parameter unsigned functionID
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_exception_handler
	probeName = HppForkHandlingProcName;
	this->HPPForkHandlingFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt32Ty(*(this->context)), //parameter pid_t 
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_exit_procedure
	probeName = noWatchPoint? HppFuncExitProcNameWithoutWatchPoint: HppFuncExitProcName;
	this->HPPFuncExitFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_enter_indeter_cs
	probeName = HppPreIndeterCallSiteProcName;
	this->HPPPreIndCallFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_exit_indeter_cs
	probeName = HppPostIndeterCallSiteProcName;
	this->HPPPostIndCallFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
//			Type::getInt32Ty(*(this->context)), //parameter unsigned indeter_call_site ID
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_enter_untrack_cs
	probeName = HppPreUntrackCallSiteProcName;
	this->HPPPreUntrackCallFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_exit_untrack_cs
	probeName = HppPostUntrackCallSiteProcName;
	this->HPPPostUntrackCallFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	probeName = HppBLPathProcName;
	this->HPPPathFunction = M.getOrInsertFunction( 
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt64Ty(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	probeName = HppWatchPointProcName;
	this->HPPWatchPointFunction = M.getOrInsertFunction( 
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt32Ty(*(this->context)),
			Type::getInt64Ty(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);
}

void HPPTransform::instrument_cleanup(Function* main) {
	BasicBlock::iterator insertPoint = (main->getEntryBlock()).getFirstInsertionPt();
	std::vector<Value*> args(1);
	args[0] = this->HPPCleanupFunction;
	CallInst::Create(this->atExitFunction, args, "", insertPoint);
}

void HPPTransform::instrument_enter_procedure(Function* func, uint32_t functionID) {
	// define the parameters to be used by the probe
	std::vector<Value*> args(1);
	args[0] = ConstantInt::get(Type::getInt32Ty(*(this->context)), functionID);

	// identify the insertion point
	BasicBlock& entryBlock = (func->getEntryBlock());
	BasicBlock::iterator insertPoint = entryBlock.getFirstInsertionPt();

	// create a CallInst at the insertion point
	CallInst::Create(this->HPPFuncEntryFunction, args, "", insertPoint);
}

// c++_updated
void HPPTransform::instrument_exception_handler(Function* func, uint32_t functionID) {
	std::vector<Value*> args(1);
	args[0] = ConstantInt::get(Type::getInt32Ty(*(this->context)), functionID);

	for (Function::iterator func_it = func->begin(), func_end = func->end(); func_it != func_end; ++func_it) {
		BasicBlock* blk = &*func_it;
		if (blk->isLandingPad()) {
			BasicBlock::iterator insertPoint = blk->getFirstInsertionPt();
			CallInst::Create(this->HPPExceptHandlingFunction, args, "", insertPoint);
		}
	}
}

void HPPTransform::instrument_exit_procedure(Function* func) {
	// locate each return instruction in the procedure
	for (Function::iterator func_it = func->begin(), func_end = func->end(); func_it != func_end; ++func_it) {
		BasicBlock* blk = &*func_it;
		TerminatorInst* terminator = blk->getTerminator();
		// c++_update
		if(isa<ReturnInst>(terminator) || isa<ResumeInst>(terminator)){
			CallInst::Create(this->HPPFuncExitFunction, "", terminator);
			// c++_update
		// we currently assume that a call or invoke inst is preceding the unreachable inst
		// and let cleanup and exception handler to deal with it
//		} else if (isa<UnreachableInst>(terminator)) {
//		//FIXME: we assume that a non-return procedure is called before each unreachable inst
//		//	&& this non-return procedure is not tracked by HPP (e.g., a library call such as exit or abort)
//			BasicBlock::iterator insertPoint = terminator;
//			--insertPoint;
//			CallInst::Create(this->HPPFuncExitFunction, "", insertPoint);
		}
	}
}

void HPPTransform::instrument_indeter_call_site(Function* func) {

//	std::unordered_set<BasicBlock*> basicBlocksFollowingInvokes;
	// iterate through all the instructions and check for CallInst and InvokeInst
	for (Function::iterator func_it = func->begin(), func_end = func->end(); func_it != func_end; ++func_it) {
		BasicBlock* blk = &*func_it;
		for (BasicBlock::iterator bb_it = blk->begin(), bb_end = blk->end(); bb_it != bb_end; ++bb_it) {
			BasicBlock::iterator insertPoint;
			if (CallInst* callInst = dyn_cast<CallInst>(&*bb_it)) {
				Function* calledFunc = callInst->getCalledFunction();
				insertPoint = callInst;
				if (!calledFunc) {
					// if indirect function call through function pointer
					instrument_enter_indeter_cs(insertPoint);
					++insertPoint;
					instrument_exit_indeter_cs(insertPoint);
				}else if (calledFunc->isDeclaration()) {
					// if function call is declared
					// in default all declared functions are not tracked
					std::unordered_set<std::string>::const_iterator iter = this->exempt_procedure_list.find(calledFunc->getName().str());
					if (iter == this->exempt_procedure_list.end()){
						instrument_enter_untrack_cs(insertPoint);
						++insertPoint;
						instrument_exit_untrack_cs(insertPoint);
					}
				}
			} else if (InvokeInst* invokeInst = dyn_cast<InvokeInst>(&*bb_it)) {
				Function* calledFunc = invokeInst->getCalledFunction();
				insertPoint = invokeInst;
				if (!calledFunc) {
					// if indirect function call through function pointer
					instrument_enter_indeter_cs(insertPoint);
					for(succ_iterator successor = succ_begin(blk),
							succEnd = succ_end(blk); successor != succEnd; ++successor ) {
						BasicBlock* succBB = *successor;
						assert(succBB->getSinglePredecessor() && "the basic block following an invoke has mutiple predecessors!");
						insertPoint = succBB->getFirstInsertionPt();
						instrument_exit_indeter_cs(insertPoint);
					}

				} else if (calledFunc->isDeclaration()) {
					// instrument the pre_untrack_cs probe before the invoke inst
					instrument_enter_untrack_cs(insertPoint);
					// keep the proceeding basic blocks of the invoke inst in a set
					for(succ_iterator successor = succ_begin(blk),
							succEnd = succ_end(blk); successor != succEnd; ++successor ) {
						BasicBlock* succBB = *successor;
						assert(succBB->getSinglePredecessor() && "the basic block following an invoke has mutiple predecessors!");
						insertPoint = succBB->getFirstInsertionPt();
						instrument_exit_untrack_cs(insertPoint);
//						basicBlocksFollowingInvokes.insert(succBB);
					}
				}
			}
		}
	}
}

void HPPTransform::instrument_fork_handler(Function* func) { 
	for (Function::iterator func_it = func->begin(), func_end = func->end(); func_it != func_end; ++func_it) {
		BasicBlock* blk = &*func_it;
		for (BasicBlock::iterator bb_it = blk->begin(), bb_end = blk->end(); bb_it != bb_end; ++bb_it) {
			BasicBlock::iterator insertPoint;
			if (CallInst* callInst = dyn_cast<CallInst>(&*bb_it)) {
				Function* calledFunc = callInst->getCalledFunction();
				insertPoint = callInst;
				if (calledFunc) {
					std::string funcName = calledFunc->getName().str();
					if (funcName == "fork" || funcName == "forkv") {
						// place instrumentation after fork or forkv
						++insertPoint;
						std::vector<Value*> args(1);
						args[0] = callInst;
						CallInst::Create(this->HPPForkHandlingFunction, args, "", insertPoint);
					}
				}
			} else if (InvokeInst* invokeInst = dyn_cast<InvokeInst>(&*bb_it)) {
				Function* calledFunc = invokeInst->getCalledFunction();
				if (calledFunc) {
					std::string funcName = calledFunc->getName().str();
					if (funcName == "fork" || funcName == "forkv") {
						// find the landingpad's normal destinated block
						insertPoint = invokeInst->getNormalDest()->getFirstInsertionPt();
						std::vector<Value*> args(1);
						args[0] = invokeInst;
						CallInst::Create(this->HPPForkHandlingFunction, args, "", insertPoint);
					}
				}
			}
		}
	}
}

void HPPTransform::instrument_enter_indeter_cs(BasicBlock::iterator insertPoint) {
	CallInst::Create(this->HPPPreIndCallFunction, "", insertPoint);
}

void HPPTransform::instrument_exit_indeter_cs(BasicBlock::iterator insertPoint/*, uint16_t indeterCallSiteID*/) {
	CallInst::Create(this->HPPPostIndCallFunction, /*args, */"", insertPoint);
}

void HPPTransform::instrument_enter_untrack_cs(BasicBlock::iterator insertPoint) {
	CallInst::Create(this->HPPPreUntrackCallFunction, "", insertPoint);
}

void HPPTransform::instrument_exit_untrack_cs(BasicBlock::iterator insertPoint) {
	CallInst::Create(this->HPPPostUntrackCallFunction, "", insertPoint);
}

// instrument the minimum set of watch points which are the call sites
void HPPTransform::instrument_watch_point(pathTracing::BLDag* dag) {
	int32_t watchPointID = 0;
	// for every node of dag
	for(pathTracing::BLNodeIterator iter = dag->getDagNodes().begin(), end = dag->getDagNodes().end();
			iter != end; ++iter) {
		pathTracing::BLNode* node = *iter;
		llvm::Value* pathStateValue = node->getStartingPathNumber();
		// the starting path number may be null for some node
		if (!pathStateValue)
			pathStateValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*(this->context)), 0);
		// skip root and exit nodes
		if (!node->getBlock())
			continue;

		for (llvm::BasicBlock::iterator bb_it = node->getBlock()->begin(), 
				bb_end = node->getBlock()->end(); bb_it != bb_end; ++bb_it) {
			bool isWatchPoint = false;
			llvm::BasicBlock::iterator wp;
			if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(&*bb_it)) {
				llvm::Function* func = callInst->getCalledFunction();
				if (!func) {
					isWatchPoint = true;
					wp = callInst;
					++watchPointID;
				} else if (!func->isIntrinsic()){
					std::unordered_set<std::string>::const_iterator iter =
						this->exempt_procedure_list.find(func->getName().str());
					// if the callsite is not our probe
					if (iter == this->exempt_procedure_list.end()){
						// callsite to be monitored
						isWatchPoint = true;
						wp = callInst;
						++watchPointID;
					}
				}
			} else if (llvm::InvokeInst* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&*bb_it)) {
				// callsite to be monitored
				isWatchPoint = true;
				wp = invokeInst;
				++watchPointID;
			}
			if (isWatchPoint) {
				std::vector<llvm::Value*> args(2);
				args[0] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(this->context)), watchPointID);
				args[1] = pathStateValue;
				llvm::CallInst::Create(this->HPPWatchPointFunction, args, "", wp);
			}
		}
	}
}

