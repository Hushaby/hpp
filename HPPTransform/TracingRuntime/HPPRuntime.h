/*
 * =====================================================================================
 *
 *       Filename:  RuntimeState.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Friday, December 27, 2013 07:27:12 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  
 *        Company:  
 *
 * =====================================================================================
 */

#ifndef HPPRUNTIME_H
#define HPPRUNTIME_H

#include <stdio.h>
#include <glib.h>
#include <pthread.h>
#include <stdint.h>

#ifdef LIBCALLOPT
typedef enum {
	TRACKING,
	IN_UNTRACK_CALL,
	HAS_UNTRACK_CALLEE
} ControlFlowState;
#endif

typedef struct trTraceState TraceState;

struct trTraceState{
	int64_t pathState;
	uint32_t watchpointID;
	// c++_updated
	uint32_t functionID;
#ifdef LIBCALLOPT
	uint32_t untrack_call_index;
	ControlFlowState cfState;
#endif
	TraceState* pred;
	TraceState* succ;
};


#define TRACE_BUFFER_SIZE 1024*64

typedef struct {
		uint32_t thread_id;
		FILE *traceLog;
		TraceState* top;
		TraceState* sentinel;
		// buffer for logging
		char trace_buffer[TRACE_BUFFER_SIZE];
} ThreadState;

#endif
