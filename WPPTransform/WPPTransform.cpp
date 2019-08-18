#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"

#include <string>
#include <vector>
#include <cstdint>

#include <unordered_set>
#include "../include/BLInstrumentation.h"
#include "TracingRuntime/tracing_runtime_interface.h"

using namespace llvm;

namespace {

class WPPTransform : public ModulePass {
	public:
		static char ID; // Pass identification, replacement for typeid
		WPPTransform() : ModulePass(ID){}
	private:
		LLVMContext* context;
		// the set of procedures which will not be treated as indeterministic call site
		// in default the set contains all probes functions inserted by WPP
		std::unordered_set<std::string> exempt_procedure_list;

		Constant* WPPFuncEntryFunction;
		Constant* WPPFuncExitFunction;
		Constant* WPPPathFunction;
		Constant* WPPPath2IndeterCSFunction;
		Constant* WPPPath2UntrackCSFunction;
		Constant* WPPPath2TrackCSFunction;

		virtual bool runOnModule(Module &M);
		// the last parameter 'bool noWatchPoint' is used to test the performance of turning off watch point
		void declareProbes(Module& M);
		void instrument_enter_procedure(Function* func, uint32_t functionID);
		void instrument_exit_procedure(Function* func);
};

} // end anonymous namespace

char WPPTransform::ID = 0;
static RegisterPass<WPPTransform> X("WPPTransform", "Transformation Pass for Bitcode instrumentation of WPP");

bool WPPTransform::runOnModule(Module &M) {

	DEBUG(dbgs()
        << "********************************************\n"
        << "********************************************\n"
        << "**                                        **\n"
        << "**          WPP INSTRUMENTATION           **\n"
        << "**                                        **\n"
        << "********************************************\n"
        << "********************************************\n");

	context = &(M.getContext());
	declareProbes(M);
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

		// NOTE: BL path instrumentation has to be performed before all other instrumentation,
		//	because it will split the Basic Block at call sites which potentially are inserted by
		//	the tracing framework
		pathTracing::BLDag* dag = pathTracing::generateWPPPathInstrumentationDag(F);
		pathTracing::setNormalBLPathProbe(this->WPPPathFunction);
		pathTracing::setPath2IndeterCSProbe(this->WPPPath2IndeterCSFunction);
		pathTracing::setPath2UntrackCSProbe(this->WPPPath2UntrackCSFunction);
		pathTracing::setPath2TrackCSProbe(this->WPPPath2TrackCSFunction);
		pathTracing::placeBLPathInstrumentation(dag, this->context, this);

		instrument_enter_procedure(F, functionID);
		instrument_exit_procedure(F);

		++functionID;
	}
	return true;
}

void WPPTransform::declareProbes(Module& M) {
	std::string probeName;

	// probe used by instrument_enter_procedure
	probeName = WppFuncEntryProcName;
	this->WPPFuncEntryFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)), //return void
			Type::getInt32Ty(*(this->context)), //parameter unsigned functionID
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	// probe used by instrument_exit_procedure
	probeName = WppFuncExitProcName;
	this->WPPFuncExitFunction = M.getOrInsertFunction(
			probeName,
			Type::getVoidTy(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	probeName = WppBLPathProcName;
	this->WPPPathFunction = M.getOrInsertFunction( 
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt64Ty(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	probeName = WppBLPath2IndeterCSProcName;
	this->WPPPath2IndeterCSFunction = M.getOrInsertFunction( 
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt64Ty(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	probeName = WppBLPath2UntrackCSProcName;
	this->WPPPath2UntrackCSFunction = M.getOrInsertFunction( 
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt64Ty(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);

	probeName = WppBLPath2TrackCSProcName;
	this->WPPPath2TrackCSFunction = M.getOrInsertFunction( 
			probeName,
			Type::getVoidTy(*(this->context)),
			Type::getInt64Ty(*(this->context)),
			NULL );
	// insert the probe into the exempt procedure list
	this->exempt_procedure_list.insert(probeName);
}

void WPPTransform::instrument_enter_procedure(Function* func, uint32_t functionID) {
	// define the parameters to be used by the probe
	std::vector<Value*> args(1);
	args[0] = ConstantInt::get(Type::getInt32Ty(*(this->context)), functionID);

	// identify the insertion point
	BasicBlock& entryBlock = (func->getEntryBlock());
	BasicBlock::iterator insertPoint = entryBlock.getFirstInsertionPt();

	// create a CallInst at the insertion point
	CallInst::Create(this->WPPFuncEntryFunction, args, "", insertPoint);
}

void WPPTransform::instrument_exit_procedure(Function* func) {
	// locate each return instruction in the procedure
	for (Function::iterator func_it = func->begin(), func_end = func->end(); func_it != func_end; ++func_it) {
		BasicBlock* blk = &*func_it;
		TerminatorInst* terminator = blk->getTerminator();
		if(isa<ReturnInst>(terminator)){
			CallInst::Create(this->WPPFuncExitFunction, "", terminator);
//		} else if (isa<UnreachableInst>(terminator)) {
//			
		//TODO: support for C++, handle procedure exit due to exception
		}
	}
}
