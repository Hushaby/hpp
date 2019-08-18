/*
 * =====================================================================================
 *
 *       Filename:  BLInstrumentation.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Thursday, May 22, 2014 09:43:01 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author: 
 *        Company:
 *
 * =====================================================================================
 */
#ifndef BLINSTRUMENTATION_H
#define BLINSTRUMENTATION_H
#include "BLDag.h"
#include "BLWPPRecoveryDag.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include <unordered_set>
#include <string>

namespace pathTracing {
	BLDag* generateHPPPathNumberingDag(llvm::Function* func);
	BLDag* generateWPPDag(llvm::Function* func);
	BLWPPRecoveryDag* generateWPPPathNumberingDag(llvm::Function* func);
	BLDag* generateHPPPathInstrumentationDag(llvm::Function* func);
	BLDag* generateWPPPathInstrumentationDag(llvm::Function* func);
	void placeBLPathInstrumentation(BLDag* dag, llvm::LLVMContext* context, llvm::Pass* pass);
//	void placeWatchPointInstrumentation(BLDag* dag, const std::unordered_set<std::string> & exempt_procedure_list,
//			llvm::Constant* watchPointProbe, llvm::LLVMContext* context);
	void setNormalBLPathProbe(llvm::Constant* p);
	void setPath2IndeterCSProbe(llvm::Constant* p);
	void setPath2UntrackCSProbe(llvm::Constant* p);
	void setPath2TrackCSProbe(llvm::Constant* p);
} // end of namespace pathTracing
#endif
