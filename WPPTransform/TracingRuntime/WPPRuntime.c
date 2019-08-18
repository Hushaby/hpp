#include "WPPRuntime.h"
#include "../../include/LogInterface.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>
#include <signal.h>
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
		g_string_append_printf(folder, "%s/blpt_%ld_%d", cwd, timer, ident);
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
	printf("WPP Tracing runtime is initialized.\n");
}

//TODO: add the support of signal handling for cleanup when program errors happen
__attribute__((destructor))
void cleanup(void) {
	for (int i=0; i<threadStates->len; ++i) {
		ThreadState* tState = g_array_index(threadStates, ThreadState*, i);
		fclose(tState->traceLog);
		//	printf("thread_%d cleans up\n", tState->thread_id);
		free(tState);
	}
	printf("Cleanup finished!\n");
}

void enterProcedure(uint32_t functionID) {
	if (!threadState) {
		ThreadState* tState = (ThreadState*)malloc(sizeof(ThreadState));

		pthread_mutex_lock(&threadCreationLock);

		// create and open trace log
		GString *traceLogName = g_string_new(folderName);
		g_string_append_printf(traceLogName, "/trace%d.log", stateIndex);
		tState->traceLog = fopen(traceLogName->str, "wb");
		g_string_free(traceLogName, TRUE);

		// set buffers for traceLog, all fully buffered
		setvbuf(tState->traceLog, tState->trace_buffer, _IOFBF, TRACE_BUFFER_SIZE);

		// append to the global threadStates
		g_array_append_val(threadStates, tState);

		++stateIndex;
		pthread_mutex_unlock(&threadCreationLock);

		threadState = tState;
	}
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Function: %d\n", functionID);
#else
	// bit version
	fwrite((const char*)(&Events[ENTER_PROC]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&functionID), FuncIDSize, 1, threadState->traceLog);
#endif
#endif
}

void exitProcedure(void) {
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Func Exit\n");
#else
	// bit version
	fwrite((const char*)(&Events[EXIT_PROC]), EventIDSize, 1, threadState->traceLog);
#endif
#endif
}

void finishBLPath(int64_t pathID){
	uint32_t pathID_32 = (uint32_t)pathID;
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Path: %d\n", pathID_32);
#else
	// bit version
	fwrite((const char*)(&Events[PATH]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&pathID_32), PathIDSize, 1, threadState->traceLog);
#endif
#endif
}

void finishBLPath2IndeterCS(int64_t pathID){
	uint32_t pathID_32 = (uint32_t)pathID;
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Path to indeterministic call site: %d\n", pathID_32);
#else
	// bit version
	fwrite((const char*)(&Events[PATH2INDETERCS]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&pathID_32), PathIDSize, 1, threadState->traceLog);
#endif
#endif
}

void finishBLPath2UntrackCS(int64_t pathID){
	uint32_t pathID_32 = (uint32_t)pathID;
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Path to untrack call site: %d\n", pathID_32);
#else
	// bit version
	fwrite((const char*)(&Events[PATH2UNTRACKCS]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&pathID_32), PathIDSize, 1, threadState->traceLog);
#endif
#endif
}

void finishBLPath2TrackCS(int64_t pathID){
	uint32_t pathID_32 = (uint32_t)pathID;
	// write to the trace file
#ifndef NONE_IO
#ifdef TEXT_LOG
	fprintf(threadState->traceLog, "Path to track call site: %d\n", pathID_32);
#else
	// bit version
	fwrite((const char*)(&Events[PATH2TRACKCS]), EventIDSize, 1, threadState->traceLog);
	fwrite((char*)(&pathID_32), PathIDSize, 1, threadState->traceLog);
#endif
#endif
}
