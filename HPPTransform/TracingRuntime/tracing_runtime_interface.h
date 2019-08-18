/*
 * =====================================================================================
 *
 *       Filename:  tracing_runtime_interface.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Monday, May 26, 2014 02:14:20 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author: 
 *        Company:
 *
 * =====================================================================================
 */
#ifndef TRACING_RUNTIME_INTERFACE_H
#define TRACING_RUNTIME_INTERFACE_H
#include <string>

//#define HPP_FUNC_ENTRY_PROC_NAME "enterProcedureWithoutWatchpoint"
//#define HPP_FUNC_ENTRY_WITH_WP_PROC_NAME "enterProcedureWithWatchpoint"
//#define HPP_FUNC_EXIT_PROC_NAME "exitProcedureWithoutWatchpoint"
//#define HPP_FUNC_EXIT_WITH_WP_PROC_NAME "exitProcedureWithWatchpoint"
//#define HPP_PRE_INDETER_CALL_SITE_PROC_NAME "preIndeterCallSite"
//#define HPP_POST_INDETER_CALL_SITE_PROC_NAME "postIndeterCallSite"
//#define HPP_BL_PATH_PROC_NAME "finishBLPath"
//#define HPP_WATCH_POINT_PROC_NAME "reachWatchPoint"

const std::string HppFuncEntryProcName = "enterProcedureWithWatchpoint";
const std::string HppFuncEntryProcNameWithoutWatchPoint = "enterProcedureWithoutWatchpoint";
const std::string HppFuncExitProcName = "exitProcedureWithWatchpoint";
const std::string HppFuncExitProcNameWithoutWatchPoint = "exitProcedureWithoutWatchpoint";
const std::string HppExceptHandlingProcName = "exceptionHandling";
const std::string HppForkHandlingProcName= "forkHandling";
const std::string HppPreIndeterCallSiteProcName = "preIndeterCallSite";
const std::string HppPostIndeterCallSiteProcName = "postIndeterCallSite";
const std::string HppPreUntrackCallSiteProcName = "preUntrackCallSite";
const std::string HppPostUntrackCallSiteProcName = "postUntrackCallSite";
const std::string HppBLPathProcName = "finishBLPath";
const std::string HppWatchPointProcName = "reachWatchPoint";
#endif
