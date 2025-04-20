/*
	oss.cpp
	Operating System Simulator

	Gavin Dennis
	CS 4760-001 Project 3
	Start date: 3/24/25
	Finish date:

	Resources/documentation consulted can be found in README

	Insert description here.
*/

#include <iostream>
#include <fstream>  // for file handling
#include <cstring>  // for cstrings and strcpy
#include <unistd.h>  // getopt, fork, exec, etc
#include <string>  // for string-to-int (stoi)
#include <sys/types.h>
#include <sys/wait.h>  // for waiting on child processes
#include <cstdlib>  // for random number generation
#include <ctime>  // time() for more accurate rng using srand
#include <sys/shm.h>  // for shared memory
#include <signal.h>  // for 60s termination
#include <sys/msg.h>  // for message queues
#include "msgbuffer.h"  // message buffer struct
#include <queue>  // for multi-level feedback queue

#define SHM_KEY1 ftok("oss.cpp", 0)
#define SHM_KEY2 ftok("oss.cpp", 1)
#define PERMS 0644  // for message queue 

#define PROCESS_TABLE_SIZE 18
#define MLFQ_SIZE 3  // size of multi-level feedback queue array

#define MAX_TIME_BETWEEN_NEW_PROCS_NS 999999999  // used to determine random time interval processes are created
#define MAX_TIME_BETWEEN_NEW_PROCS_SECS 1

#define ALARM_TIME 3  // how long before program gets signal to terminate

struct PCB {
	// process control blocks
	
	bool occupied = false;  // flag for whether or not a child process is in pcb
	pid_t pid = 0;  // the process id of the child;
	int startSeconds = 0;  // time according to shared memory that child was created
			       // (e.g. 1 if the time was 1.000425);
	int startNano = 0; // time according to shared memory that child was created 
			   // (e.g. .000425 if the time was 1.000425);
	int serviceTimeSeconds;  // total seconds process has been scheduled
	int serviceTimeNano;  // total nanoseconds process has been scheduled
	int eventWaitSec;  // How many seconds until event happens
	int eventWaitNano;  // How many nanosec until event happens
	bool blocked;  // flag that specifies if process is waiting on event
};

// global variables for shared memory, log file, process table, and message queue
// shared memory
unsigned int shmClockS;
unsigned int shmClockN;
unsigned int * shmClockSPtr;
unsigned int * shmClockNPtr;

// log file
std::ofstream logFile; 
int lineNum = 0;

// message queue
int msqid;

// process table
PCB processTable[PROCESS_TABLE_SIZE];  // process table: array of PCBs that keep track of children
int clockTarget = 500000000;  // used for outputting process table every half second

// multi level feedback queue with high, medium, and low priority
std::queue<int> MLFQ[MLFQ_SIZE];
int timeQuantam[MLFQ_SIZE] = {
	// time quantams for each queue (highest to lowest)
	10000000,  // 10ms
	20000000,  // 20ms
	40000000,  // 40ms
};

void showHelp() {
	// displays help message to user when prompted with -h. then terminates
	
	std::cout << "Operating Systems - Project 2\n\n"
		  << "Creates a simulated clock in shared memory and launches child tasks who behave according to program parameters and the aforementioned clock.\n\n"
		  << "Takes in four arguments:\n"
		  << "\t-n: number of child process to launch (e.g. 5)\n"
		  << "\t-s: how many child processes can run simultaneously (e.g. 3)\n"
		  << "\t-t: time limit for children in seconds (e.g. 2)\n"
		  << "\t-i: how often a child should be launched in ms, based on simulated clock (e.g. 100)\n\n"
		  << "The program will terminate if nonnegative integers are not provided.\n"
		  << "Floating-point numbers will be rounded to an integer.\n"
		  << "Exiting program.\n";

	return;
}

void outputProcessTable() {
	// prints out contents of the process table and MLFQ
	// and writes to log file
	
	// process table
	std::cout << "**************************************************\n"  
		  << "OSS PID:" << getpid() << " SysClockS:" << *shmClockSPtr << " SysClockNano:" << *shmClockNPtr << "\n"
		  << "Process Table: \n"
		  << "Entry \t" << "Occupied \t" << "PID \t" << "StartS \t" << "StartN \n";
	for (int i = 0; i < PROCESS_TABLE_SIZE; i++) { 
		// output every entry of the table
		std::cout << i << "\t" << processTable[i].occupied << "\t\t" << processTable[i].pid << "\t" << processTable[i].startSeconds 
			  << "\t" << processTable[i].startNano << "\n";
	}

	// mlfq
	std::cout << "\nMulti Level Feedback Queue:\n";
	std::queue<int> tempMLFQ[MLFQ_SIZE];  // create a temporary copy of the queue 
	for (int i = 0; i < MLFQ_SIZE; i++) {
		tempMLFQ[i] = MLFQ[i];
		std::cout << "Queue " << i << ": ";
		while (!tempMLFQ[i].empty()) {
			// keep printing out the front of the queue, then pop the front off
			std::cout << tempMLFQ[i].front() << " ";
			tempMLFQ[i].pop();
		}
		std::cout << "\n";
	}

	std::cout << "**************************************************\n"; 
	
	// write output to log file
	if (lineNum > 10000) {
		return;
	}

	// make sure log is open before writing
	if (!logFile.is_open()) {
		std::cerr << "ERROR: Unable to write to log file.\n" << " Terminating program.\n";
		exit(EXIT_FAILURE);
	}

	// process table
	logFile << "**************************************************\n"  
	    << "OSS PID:" << getpid() << " SysClockS:" << *shmClockSPtr << " SysClockNano:" << *shmClockNPtr << "\n"
 	    << "Process Table: \n"
	    << "Entry \t" << "Occupied \t" << "PID \t" << "StartS \t" << "StartN \n";
	lineNum += 3;
	for (int i = 0; i < PROCESS_TABLE_SIZE; i++) { 
		// output every entry of the table
		logFile << i << "\t" << processTable[i].occupied << "\t\t" << processTable[i].pid << "\t" << processTable[i].startSeconds << "\t" 
		    << processTable[i].startNano << "\n";
		lineNum++;
	}

	// mlfq
	logFile << "\nMulti Level Feedback Queue:\n";
	for (int i = 0; i < MLFQ_SIZE; i++) {
		tempMLFQ[i] = MLFQ[i];
 		logFile << "Queue " << i << ": ";
		lineNum++;
		while (!tempMLFQ[i].empty()) {
			// keep printing out the front of the queue, then pop the front off
			logFile << tempMLFQ[i].front() << " ";
			tempMLFQ[i].pop();
			lineNum++;
		}
		logFile << "\n";
	}
	
	logFile << "**************************************************\n";  // lines for readability
	lineNum++;

	return;

}

void incrementClock(int nanoAmount) {
	/* increments simulated clock in shared memory by
	a given number of nanoseconds  */
	
	int secAmount = 0;
	while (nanoAmount >= 1000000000) {
		nanoAmount -= 1000000000;
		secAmount++;
	}

	// increment the actual clock
	*shmClockSPtr += secAmount;
	*shmClockNPtr += nanoAmount;
	while (*shmClockNPtr >= 1000000000) {
		*shmClockNPtr -= 1000000000;
		*shmClockSPtr++;
	}

	return;
}

void signalHandler(int sig) {
	// frees memory and terminates process + children after 60sec
	
	// terminate the children
	for (int i = 0; i < PROCESS_TABLE_SIZE; i++) {
		// check the process table to see whose death is awaiting

		if (!processTable[i].occupied)
			continue;
		kill(processTable[i].pid, SIGKILL);
	}

	// detach and free memory
	shmdt(shmClockSPtr);
	shmdt(shmClockNPtr);
	shmctl(shmClockS,IPC_RMID, NULL);
	shmctl(shmClockN,IPC_RMID, NULL);

	// get rid of message queue
	msgctl(msqid, IPC_RMID, NULL);

	// close the log file
	if (logFile.is_open()) {
		logFile.close();
	}

	// output message
	std::cerr << "TIMEOUT: Terminating program.\n";

	// terminate
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
	
	// turn on alarm handler
	signal(SIGINT, signalHandler);  // ctr-c signal
	signal(SIGALRM, signalHandler);  // alarm signal
	alarm(ALARM_TIME);  // kill after 60s

	// declare the important stuff
	int proc;  // number of total children to launch
	int simul;  // how many processes can be ran simultaneously
	int timeLimit;  // how long children should run before being terminated
	int interval; // how often a child should be launched in milliseconds
	std::string logFileName;  // name of log file to write to

	int procCreated = 0;  // number of children that have been created
	int procRunning = 0;  // number of children currently running	

	int totalMessages = 0;  // total messages sent for summary output

	// create shared memory and attach
	shmClockS = shmget(SHM_KEY1, sizeof(int), IPC_CREAT | 0666);  // simulated clock (sec)
	shmClockN = shmget(SHM_KEY2, sizeof(int), IPC_CREAT | 0666);  // simulated clock (nano)
	if (shmClockS == -1 || shmClockN == -1) {
		// print an error message and halt if shared memory fails

		std::cerr << "ERROR: Shared memory failed.\n" << "Terminating program.\n";
		return 1;
	}		
	shmClockSPtr = (unsigned int *)shmat(shmClockS, 0, 0);
	shmClockNPtr = (unsigned int *)shmat(shmClockN, 0, 0);
	if (shmClockSPtr <= 0 || shmClockNPtr <= 0) {
		// print an error message and halt if shared memory pointer fails
		
		std::cerr << "ERROR: Failed to create pointers.\n" << "Terminating program.\n"; 
		return 1;
	}
	
	// set the clock to 0
	*shmClockSPtr = 0;  
	*shmClockNPtr = 0;

	// setup message queue stuff
	msgbuffer buf; 
	key_t key;
	
	// get key for message queue
	system("touch msgq.txt");
	if ((key = ftok("msgq.txt", 1)) == -1) {
		std::cerr << "ERROR: Failed to generate key for message queue. Terminating\n";
		exit(EXIT_FAILURE);
	}
	
	// create the actual message queue
	if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
		std::cerr << "ERROR: Failed to create message queue in oss. Terminating program.\n";
		exit(EXIT_FAILURE);
	}
	std::cout << "Message queue succesfully created.\n";

	// parse command-line options
	int option;
	const char optstr[] = "hf";
	while ((option = getopt(argc, argv, optstr)) != -1) {
		switch (option) {
			case 'h':
				// help message
				showHelp();
				return 0;
			case 'f':
				// log file
				logFileName = optarg;
				break;
			default:
				// invalid option message
				std::cout << "Use -h command for help.\n" << "Exiting program.\n";
				return 1;
		}
	}

	// open the log file
	if (logFileName.empty()) { 
		logFileName = "log.txt";
	}
	logFile.open(logFileName, std::ofstream::trunc);
	if (!logFile.is_open()) {
		std::cerr << "ERROR: Unable to write to log file.\n" << " Terminating program.\n";
		exit(EXIT_FAILURE);
	}

	// time intervals between user process creation
	srand(time(0));  // seed for rng
	const int timeBetweenNewProcsSecs = rand() % MAX_TIME_BETWEEN_NEW_PROCS_SECS; 
	const int timeBetweenNewProcsNS = rand() % MAX_TIME_BETWEEN_NEW_PROCS_NS;
	std::cout << "Time between new procs: " << timeBetweenNewProcsSecs << ":" << timeBetweenNewProcsNS << "\n";

	// keep track of when the next process can be created and 
	// when the process table should be outputted
	int nextTimeS = timeBetweenNewProcsSecs;
	int nextTimeN = timeBetweenNewProcsNS;
	int outputTimeS = 0;
	int outputTimeN = 500000000;

	// create and schedule processes
	while (procCreated < 1 || procRunning > 0) {
		
		// if all the processes have been made, keep looping until they've all terminated
		if (procCreated == proc) {
			continue;
		}

		// fork next child if clock is ready and there are still processes to make
		if (procCreated < 1 && *shmClockSPtr >= nextTimeS && *shmClockNPtr >= nextTimeN) {
			pid_t childPID = fork();
			procCreated++;
			procRunning++;
			if (childPID < 0) {
				// unsuccessful fork
				std::cerr << "ERROR: Failed to fork.\n" << "Terminating program.\n";
				return 1;
			}
			else if (childPID == 0) {
				// child process

				// run worker executable
				const char * arg0 = "./worker";  // arg0 is the worker executable
				execlp(arg0, arg0, nullptr);
				
				// if exec fails for whatever reason, throw an error and terminate
				std::cerr << "ERROR: Failed to execute \"./worker\".\n" << "Terminating program. \n";
				return 1;
			}
			else {
				// parent process

				// add child to process table and MLFQ
				int tableIndex;

				for (int i = 0; i < PROCESS_TABLE_SIZE; i++) {
					// find the earliest open index in the table

					if (!processTable[i].occupied) {
						tableIndex = i;
						break;
					}
				}
				
				// fill out the process control block
				std::cout << "OSS: Generating process with PID " << childPID << " and putting it in queue 0 at time " << *shmClockSPtr << ":" << *shmClockNPtr << "\n";
				logFile << "OSS: Generating process with PID " << childPID << " and putting it in queue 0 at time " << *shmClockSPtr << ":" << *shmClockNPtr << "\n";
				processTable[tableIndex].occupied = true;
				processTable[tableIndex].pid = childPID; 
				processTable[tableIndex].startNano = *shmClockNPtr;
				processTable[tableIndex].startSeconds = *shmClockSPtr;
				processTable[tableIndex].serviceTimeSeconds = 1;
				processTable[tableIndex].serviceTimeNano = 1;
				processTable[tableIndex].eventWaitSec = 0;
				processTable[tableIndex].eventWaitNano = 0;
				processTable[tableIndex].blocked = false;

				// add child to mlfq
				MLFQ[0].push(childPID);
				std::cout << childPID << " added to MLFQ" << 0 << "\n";
				logFile << childPID << " added to MLFQ" << 0 << "\n";
			}
		}

		// check if blocked process should be changed to ready
		// if so put it in highest priority queue

		// if there is at least one ready process, schedule the process in highest queue
		// by sending it a message, then remove it from the queue
		// send the message and print to the screen/log file
		int childToMessage = -1;
		for (int i = 0; i < MLFQ_SIZE; i++) {
			if (MLFQ[i].empty()) {
				continue;
			}

			// select the first available child process
			childToMessage = MLFQ[i].front();
			MLFQ[i].pop();
			break;
		}
		if (childToMessage != -1) {
			std::cout << "OSS: Sending message to worker " << childToMessage << " at time " << *shmClockSPtr << ":" << *shmClockNPtr << "\n";
			if (msgsnd(msqid, &buf, sizeof(msgbuffer) - sizeof(long), 0) == -1) {
				std::cerr << "ERROR: OSS failed to send message to child " 
					  << childToMessage << ".\n";
				exit(EXIT_FAILURE);
			}
			childToMessage = -1;
		}

		// receive message back and update process table
		msgbuffer rcvbuf;
		if (msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1) {
			std::cerr << "ERROR: OSS failed to receive message from child " 
				  << childToMessagePID << ".\n";
			exit(EXIT_FAILURE);
		}
		// print out the message receipt
		std::cout << "OSS: Receiving message from worker " << childToMessage << " PID "
			  << childToMessagePID << " at time " << *shmClockSPtr << ":"
			  << *shmClockNPtr << "\n";
		logFile << "OSS: Receiving message from worker " << childToMessage << " PID "
			<< childToMessagePID << " at time " << *shmClockSPtr << ":"
			<< *shmClockNPtr << "\n";

		// increment clock by 100ms
		incrementClock(100000000);

		// every half a second, output the process table
		if ((*shmClockSPtr >= outputTimeS && *shmClockNPtr >= outputTimeN) || *shmClockSPtr > outputTimeS) {
			outputTimeN += 500000000;
			if (outputTimeN >= 1000000000) {
				outputTimeN = 0;
				outputTimeS++;
			}
			outputProcessTable();
		}
	}
	std::cout << "Finished making processes \n";

	// detatch from shared memory and free it
	shmdt(shmClockSPtr);
	shmdt(shmClockNPtr);
	shmctl(shmClockS,IPC_RMID, NULL);
	shmctl(shmClockN,IPC_RMID, NULL);

	// get rid of message queue
	msgctl(msqid, IPC_RMID, NULL);

	// program is finished
	logFile.close();  // close the log file
	std::cout << "OSS: Finished.\n" << "Terminating program.\n";
	return 0;
}
