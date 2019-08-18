/*
 * =====================================================================================
 *
 *       Filename:  WPPRuntime.h
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

#ifndef WPPRUNTIME_H
#define WPPRUNTIME_H

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#define TRACE_BUFFER_SIZE 1024*64

typedef struct {
		uint32_t thread_id;
		FILE *traceLog;
		// buffer for logging
		char trace_buffer[TRACE_BUFFER_SIZE];
} ThreadState;

#endif
