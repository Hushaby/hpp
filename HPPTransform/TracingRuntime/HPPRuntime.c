#include "HPPRuntime.h"
#include "../../include/LogInterface.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
// OPE_DEBUG
//#include <limits.h>

// thread local pointer referencing the threadState of each thread
__thread ThreadState *threadState;

// global variables
gchar* folderName;
GArray* threadStates;
pthread_mutex_t threadCreationLock;
unsigned stateIndex;

// using the gcc/clang's specific variable
__attribute__((constructor))
void initRuntime(void) {
	//init the folder name
	// use the current time to differ log files
	time_t timer;
	time(&timer);
	pid_t ident = getpid();
	GString *folder = g_string_new(NULL);
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL)
		g_string_append_printf(folder, "%s/hpp_%ld_%d", cwd, timer, ident);
	else 
		exit(EXIT_FAILURE);

	int stat = mkdir(folder->str, 0777);
	// if the folder with the same name exists, append _0 to differ
	while (stat == -1 && errno == EEXIST){
		g_string_append(folder, "_0");
		stat = mkdir(folder->str, 0777);
	}
	folderName = g_string_free(folder, FALSE);
	threadStates = g_array_new(FALSE, TRUE, sizeof(void*));
	pthread_mutex_init(&threadCreationLock, NULL);
	stateIndex = 0;
	printf("HPP Tracing runtime is initialized.\n");
}

//TODO: add the support of signal handling for cleanup when program errors happen
__attribute__((destructor))
void cleanupWithWatchPoint(void) {
	for (int i=0; i<threadStates->len; ++i) {
		ThreadState* tState = g_array_index(threadStates, ThreadState*, i);
		if (!tState) {
			continue;
		}
		while (tState->top != tState->sentinel) {
			TraceState *traceState = tState->top;
#ifndef NONE_IO
			// write to the trace file
#ifdef TEXT_LOG
			fprintf(tState->traceLog, "Watchpoint: %u | PathState: %ld\n", 
					traceState->watchpointID, traceState->pathState);
//			fprintf(tState->traceLog, "Watchpoint: %u | PathState: %ld | FunctionID: %u\n", 
//					traceState->watchpointID, traceState->pathState, traceState->functionID);
#else
			// bit version
			fwrite((const char*)(&Events[UNFINISHED_PATH]), EventIDSize, 1, tState->traceLog);
			fwrite((char*)(&traceState->watchpointID), WatchPointIDSize, 1, tState->traceLog);
			fwrite((char*)(&traceState->pathState), PathStateSize, 1, tState->traceLog);
#endif
#endif
			tState->top = traceState->pred;
		}
		fclose(tState->traceLog);
		//	printf("thread_%d cleans up\n", tState->thread_id);
		free(tState);
	}
	printf("Cleanup finished!\n");
}

void enterProcedureWithWatchpoint(uint32_t functionID) {
	if (!threadState) {
		pid_t pid = getpid();
		ThreadState* tState = (ThreadState*)malloc(sizeof(ThreadState));

		pthread_mutex_lock(&threadCreationLock);

		// create and open trace log
		GString *traceLogName = g_string_new(folderName);
		g_string_append_printf(traceLogName, "/trace%d.log", stateIndex);
		tState->thread_id = stateIndex;
		tState->traceLog = fopen(traceLogName->str, "wb");
		g_string_free(traceLogName, TRUE);

		// set buffers for traceLog, all fully buffered
		setvbuf(tState->traceLog, tState->trace_buffer, _IOFBF, TRACE_BUFFER_SIZE);

		// init the bookkeeping state stack which is implemented by a simple linked list
		tState->sentinel = (TraceState*)malloc(sizeof(TraceState));
#ifdef LIBCALLOPT
		tState->sentinel->cfState = TRACKING;
#endif
		tState->sentinel->succ = NULL;
		tState->top = tState->sentinel;
		
		// append to the global threadStates
		g_array_append_val(threadStates, tState);

		++stateIndex;
		pthread_mutex_unlock(&threadCreationLock);

		threadState = tState;
	}
#ifdef LIBCALLOPT
	if (threadState->top->cfState == IN_UNTRACK_CALL) {
		threadState->top->cfState = HAS_UNTRACK_CALLEE;
		// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
		fprintf(threadState->traceLog, "IndCall Starts\n");
#else
		// bit version
		fwrite((const char*)(&Events[ENTER_UNTRACK_CS]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
	}
#endif
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Function: %u\n", functionID);
#else
	// bit version
	fwrite((const char*)(&Events[ENTER_PROC]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&functionID), FuncIDSize, 1, threadState->traceLog);
#endif
#endif
	// update the traceStateStack
	TraceState* newTraceState = threadState->top->succ;
	if (!newTraceState) {
		newTraceState = (TraceState*)malloc(sizeof(TraceState));
		newTraceState->succ = NULL;
		newTraceState->pred = threadState->top;
		threadState->top->succ = newTraceState;
	}
	threadState->top = newTraceState;
	newTraceState->pathState=0;
	newTraceState->watchpointID=0;
	newTraceState->functionID=functionID;
#ifdef LIBCALLOPT
	newTraceState->untrack_call_index = 0;
	newTraceState->cfState = TRACKING;
#endif
}

void exitProcedureWithWatchpoint(void) {
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Func Exit\n");
#else
	// bit version
	fwrite((const char*)(&Events[EXIT_PROC]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
	threadState->top = threadState->top->pred;
	assert(threadState->top && "proc exit probes are executed more than proc enter probes");
}

void finishBLPath(int64_t pathID){
  // FIXME: this assertion fails on 400.perlbench function 1620
  //assert(pathID < UINT_MAX);
	uint32_t pathID_32 = (uint32_t)pathID;
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Path: %ld\n", pathID);
#else
	// bit version
	fwrite((const char*)(&Events[PATH]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&pathID_32), PathIDSize, 1, threadState->traceLog);
#endif
#endif
#ifdef LIBCALLOPT
	threadState->top->untrack_call_index = 0;
#endif
}

void enterProcedureWithoutWatchpoint(uint32_t functionID) {
}

void exitProcedureWithoutWatchpoint(void) {
}

// c++_updated
void exceptionHandling(uint32_t functionID) {

	assert(threadState->top != threadState->sentinel);
	TraceState* traceState = threadState->top;
	while (traceState->functionID != functionID) {
#ifndef NONE_IO
		// write to the trace file
#ifdef TEXT_LOG
		fprintf(threadState->traceLog, "Watchpoint: %u | PathState: %ld\n", 
				traceState->watchpointID, traceState->pathState);
#else
		// bit version
		fwrite((const char*)(&Events[UNFINISHED_PATH]), EventIDSize, 1, threadState->traceLog);
		fwrite((char*)(&traceState->watchpointID), WatchPointIDSize, 1, threadState->traceLog);
		fwrite((char*)(&traceState->pathState), PathStateSize, 1, threadState->traceLog);
#endif
#endif
		traceState = traceState->pred;
		threadState->top = traceState;
	}
}

void forkHandling(uint32_t pid) {
	if (pid == 0) {
		// this is the child process
		// create a new folder inside the original folder
		pid_t ident = getpid();
		GString *newFolder = g_string_new(folderName);
		g_free(folderName);
		g_string_append_printf(newFolder, "/%d", ident);

		int stat = mkdir(newFolder->str, 0777);
		// if the folder with the same name exists, append _0 to differ
		while (stat == -1 && errno == EEXIST){
			g_string_append(newFolder, "_0");
			stat = mkdir(newFolder->str, 0777);
		}
		folderName = g_string_free(newFolder, FALSE);

		// discard all the buffers of trace logs
		// remain the current threadState and remove others
		for (int i=0; i<threadStates->len; ++i) {
			ThreadState* tState = g_array_index(threadStates, ThreadState*, i);
			if (tState) {
				__fpurge(tState->traceLog);
				fclose(tState->traceLog);
				if (tState != threadState) {
					free(tState);
				}
			}
		}
		// replace the trace file handler with new trace file for each threadState
		GString *traceLogName = g_string_new(folderName);
		g_string_append_printf(traceLogName, "/trace%d.log", threadState->thread_id);
		threadState->traceLog = fopen(traceLogName->str, "wb");
		g_string_free(traceLogName, TRUE);
	} else {
		// this is the parent process
#ifndef NONE_IO
		// write to the trace file
#ifdef TEXT_LOG
		fprintf(threadState->traceLog, "fork process: %u\n", pid);
#else
		// bit version
		fwrite((const char*)(&Events[FORK]), EventIDSize, 1, threadState->traceLog);
		fwrite((char*)(&pid), ForkPIDSize, 1, threadState->traceLog);
#endif
#endif
	}
}

void preUntrackCallSite(void) {
#ifdef LIBCALLOPT
	threadState->top->cfState = IN_UNTRACK_CALL;
#else
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "UntrackCall Starts\n");
#else
	// bit version
	fwrite((const char*)(&Events[ENTER_UNTRACK_CS]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
#endif
}

void postUntrackCallSite(void) {
#ifdef LIBCALLOPT
	if (threadState->top->cfState == HAS_UNTRACK_CALLEE) {
		uint32_t index = threadState->top->untrack_call_index;
		// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
		fprintf(threadState->traceLog, "UntrackCall Ends with untrack index: %u\n", index);
#else
		// bit version
		fwrite((const char*)(&Events[EXIT_UNTRACK_CS]), EventIDSize, 1, threadState->traceLog);
		fwrite((char*)(&index), UntrackCallIndexSize, 1, threadState->traceLog);
#endif
#endif
		++threadState->top->untrack_call_index;
	}
	threadState->top->cfState = TRACKING;
#else
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "IndCall Ends\n");
#else
	// bit version
	fwrite((const char*)(&Events[EXIT_UNTRACK_CS]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
#endif
}

void preIndeterCallSite(void) {
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "IndCall Starts\n");
#else
	// bit version
	fwrite((const char*)(&Events[ENTER_INDETER_CS]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
}

void postIndeterCallSite(void) {
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "IndCall Ends\n");
#else
	// bit version
	fwrite((const char*)(&Events[EXIT_INDETER_CS]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
}

void reachWatchPoint(uint32_t watchpointID, int64_t pathState){
	// update the current tracestate
	TraceState *traceState = threadState->top;
	traceState->watchpointID = watchpointID;
	traceState->pathState = pathState;
}
