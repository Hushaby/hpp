/*
 * =====================================================================================
 *
 *       Filename:  LogInterface.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Saturday, December 28, 2013 05:11:10 HKT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  
 *        Company: 
 *
 * =====================================================================================
 */
#ifndef LOGINTERFACE_H
#define LOGINTERFACE_H

const unsigned FuncIDSize = 4; //bytes
const unsigned WatchPointIDSize = 4; //bytes
const unsigned PathIDSize = 4; //bytes
const unsigned PathStateSize = 8; //bytes
const unsigned EventIDSize = 1; //byte
const unsigned UntrackCallIndexSize = 4; //bytes
const unsigned ForkPIDSize = 4; //bytes

enum EventType {
	ENTER_PROC = 0,
	EXIT_PROC,
	PATH,
	ENTER_UNTRACK_CS,
	EXIT_UNTRACK_CS,
	ENTER_INDETER_CS,
	EXIT_INDETER_CS,
	UNFINISHED_PATH,
	FORK,
	PATH2TRACKCS,
	PATH2INDETERCS,
	PATH2UNTRACKCS
};

const char Events[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};

#endif
