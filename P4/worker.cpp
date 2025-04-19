/*
	worker.cpp

	takes in two command line args corresponding to the max time it should decide
	to stay around in the system. It then looks at shared memory to determine
	what time it should terminate.
*/

#include <iostream>
#include <string>  // for string-to-int (stoi)
#include <cstring>  // for strcpy and c-strings
#include <unistd.h>  // for pid and such
#include <sys/shm.h>  // for shared memory
#include <sys/msg.h>  // for message queues
#include "msgbuffer.h"  // message buffer struct
#include <ctime>  // time() for more accurate rng using srand

#define SHM_KEY1 ftok("oss.cpp", 0)
#define SHM_KEY2 ftok("oss.cpp", 1)
#define PERMS 0644

int temp = 0;

int main(int argc, char** argv) {
	
	// get and attach to shared memory
	int shmClockS = shmget(SHM_KEY1, 0, 0);
	int shmClockN = shmget(SHM_KEY2, 0, 0);
	if (shmClockS == -1 || shmClockN == -1) {
		// validate shared memory get was successful
		std::cerr << "ERROR: Shared memory failed.\n" << "Terminating program.\n";
		return 1;
	}
	int * shmClockSPtr = (int *)shmat(shmClockS, 0, 0);
	int * shmClockNPtr = (int *)shmat(shmClockN, 0, 0);
	if (shmClockSPtr <= 0 || shmClockNPtr <= 0) {
		// validate pointer initialization
		std::cerr << "ERROR: Unable to initialize shared memory pointers.\n" << "Terminating program.\n";
		return 1;
	}
	
	// set up message queue
	msgbuffer buf;
	buf.mtype = 1;
	int msqid = 0;
	key_t key;

	if ((key = ftok("msgq.txt", 1)) == -1) {
		// print an error message and halt if key isn't generated
		std::cerr << "ERROR: Message queue key not generated. Terminating program.\n";
		exit(EXIT_FAILURE);
	}

	if ((msqid = msgget(key, PERMS)) == -1) {
		// create the message queue and throw error if unsuccessful
		std::cerr << "ERROR: Unable to create message queue. Terminating program.\n";
		exit(EXIT_FAILURE);
	}

	// startup message for debugging
	std::cout << "Starting but not running: " << getpid() << "\n";

	// set up variables for termination time
	int termTimeS = -1;
	int termTimeN = -1;

	// decide what process will do after time quantum is finished
	srand(time(0));
	int random = rand() % 100;
	std::string method;
	if (random <= 90) {
		// process will be interrupted and sent to next queue
		method = "interrupt";
	}
	else if (random <= 95) {
		// process will be blocked and returned to highest queue
		method = "i/o";
	}
	else {
		// process is finished and will terminate
		method = "terminate";
	}

	// wait for oss message, then check clock to see if it's time to terminate
	// repeat until it's time
	do {
		// wait for oss message
		if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
			std::cerr << "Failed to receive message from parent. Terminating program.\n";
			exit(EXIT_FAILURE);
		}
		else {
			int timeQuantum = buf.intData;
			termTimeS = *shmClockSPtr;
			termTimeN = *shmClockNPtr + timeQuantum * 1000000;
			if (termTimeN >= 1000000000) {
				termTimeS++;
				termTimeN -= 1000000000;
			}
			std::cout << "Clock time: " << *shmClockSPtr << ":" << *shmClockNPtr << "\nterm time: " << termTimeS << ":" << termTimeN << "\n";
		}
		
		// check the clock, determine if TtT, and output message
		if (termTimeS != -1 && *shmClockSPtr >= termTimeS && *shmClockNPtr >= termTimeN) {
			// send message to oss with status (full time, interrupt, or terminated)
			std::cout << "sending message to oss \n";
			buf.mtype = getppid();
			strcpy(buf.strData, method.c_str());
			if (msgsnd(msqid, &buf, sizeof(msgbuffer) - sizeof(long), 0) == -1) {
				// print error message if msgsend fails
				std::cerr << "ERROR: Failed to message parent process. Terminating program.\n";
				exit(EXIT_FAILURE);
			}
		}

		temp++;
		if (temp >= 9999999999) {
			std::cout << "breaking from temp \n";
			break;
		}
	} while (true);

	// detach from shared memory
	shmdt(shmClockSPtr);
	shmdt(shmClockNPtr);
	
	// program is finished
	return 0;
}
