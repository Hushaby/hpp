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
#ifndef WPP_TRACING_RUNTIME_INTERFACE_H
#define WPP_TRACING_RUNTIME_INTERFACE_H
#include <string>

const std::string WppFuncEntryProcName = "enterProcedure";
const std::string WppFuncExitProcName = "exitProcedure";
const std::string WppBLPathProcName = "finishBLPath";
const std::string WppBLPath2IndeterCSProcName = "finishBLPath2IndeterCS";
const std::string WppBLPath2UntrackCSProcName = "finishBLPath2UntrackCS";
const std::string WppBLPath2TrackCSProcName = "finishBLPath2TrackCS";
#endif
