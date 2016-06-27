/*
 * readcommand.h
 *
 *  Created on: 13.01.2010
 *      Author: Oliver Jack
 */

#ifndef DOUBLE_INCLUDE_CHECK_READCOMMAND
#define DOUBLE_INCLUDE_CHECK_READCOMMAND

//MessageQueue
#define MEG_Q_Job_MAX_Messages 200
MSG_Q_ID mesgQueueIdJob;

typedef enum {false, true} bool;
typedef struct {
	bool parse_ok;
	char cmd[20];
	int par1;
	int par2;
} command;

command readcommand(char *cmd);

#endif /* DOUBLE_INCLUDE_CHECK_READCOMMAND_H_ */
