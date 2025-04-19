/*
	msgbuffer.h

	header file for msgbuffer struct
*/

#ifndef MSGBUFFER_H
#define MSGBUFFER_H

struct msgbuffer {
	long mtype;
	char strData[100];
	int intData;
};

#endif
