/*


Description: Fround-robbin (quad-core) CPU scheduling simulator.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "sch-helpers.h"

/* Below are global variables and structs to be used by FCFS scheduler */

process process_frm_datafile[MAX_PROCESSES+1]; /* a large array to hold all processes read from data file */
/* these processes are pre-sorted and ordered by arrival time */

int totalProcesses; /* total number of processes */
int nextProcess; /* index of next process to arrive */
process_queue readyQ; /* ready queue to hold all ready processes */
process_queue ioWaitingQ; /* waiting queue to hold all processes in I/O waiting */
process *cpu[NUMBER_OF_PROCESSORS]; /* processes running on each cpu */
int totalWaitingTime; /* total time processes spent waiting */
int totalContextSwitches; /* total number of preemptions */
int clock_cycle; /* time steps simulated */
int cpuTimeUtilized; /* time steps each cpu was executing */
int tQuantum;

/* holds processes moving to the ready queue this timestep (for sorting) */
process *preReadyQ[MAX_PROCESSES];
int preReadyQSize;


/* variable initialization */
void initializeGlobals(void) 
{
    for (int i=0; i<NUMBER_OF_PROCESSORS; i++) 
    {
        cpu[i] = NULL;
    }

    clock_cycle = 0;
    cpuTimeUtilized = 0;
    totalWaitingTime = 0;
    totalContextSwitches = 0;
    totalProcesses = 0;
    nextProcess = 0;
    tQuantum = 0;
    preReadyQSize = 0;

    initializeProcessQueue(&readyQ);
    initializeProcessQueue(&ioWaitingQ);
}

/* compares processes pointed to by *aa and *bb by process id,
returning -1 if aa < bb, 1 if aa > bb and 0 otherwise. */
int compareProcessPointers(const void *aa, const void *bb) 
{
    process *a = *((process**) aa);
    process *b = *((process**) bb);
    if (a->pid < b->pid) return -1;
    if (a->pid > b->pid) return 1;
    assert(0);
    return 0;
}

/* returns the no of processes using the cpus */
int runningProcesses(void) 
{
    int result = 0;
    int i;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] != NULL) result++;
    }
    return result;
}

/* returns the no of processes pending arrival to system */
int incomingProcesses(void) 
{
    return totalProcesses - nextProcess;
}


/**
 * checks the ready queue for processes ready to send to cpu for execution, and returns a pointer to the next process in queue
 * if ready queue is empty returns null
*/
process *nextScheduledProcess(void) 
{
    if (readyQ.size == 0)
    {
        return NULL;
    }
    process *result = readyQ.front->data;
    dequeueProcess(&readyQ);
    return result;
}

/* 1 st step: adding newly arriving processes to the preReadyQ */
void enquePreReadyQ(void) 
{
    /* newly arriving processes added to an intermediate array unsorted,
    they will be sorted and added to the ready queue */
    while (nextProcess < totalProcesses && process_frm_datafile[nextProcess].arrivalTime <= clock_cycle) 
    {
        preReadyQ[preReadyQSize] = &process_frm_datafile[nextProcess];
        preReadyQ[preReadyQSize]->quantumRemaining = tQuantum;
        preReadyQSize++;
        nextProcess++;
    }
}


/**
 * check IO waiting queue, if any process has reached the IO burst length, move to the preReady queue for sorting
 * thereafter the process will be queued in the readyQ
 * if process has not reached IO burst length, enqueue process back to the IOwait queue
 */
void fromIOWaitingToPreReady(void) 
{
    int size = ioWaitingQ.size;

    for (int i=0;i<size;i++) 
    {
        process *front = ioWaitingQ.front->data; 
        dequeueProcess(&ioWaitingQ); 

        assert(front->bursts[front->currentBurst].step <= front->bursts[front->currentBurst].length);

        if (front->bursts[front->currentBurst].step == front->bursts[front->currentBurst].length) 
        {
            front->currentBurst++;	//Move to the next burst by updating current burst counter.
			//front->quantumRemaining = tQuantum; //
			front->endTime = clock_cycle;
			preReadyQ[preReadyQSize++] = front; 
        } 
        else 
        {
            enqueueProcess(&ioWaitingQ, front);
        }
    }
}


/**
 * sort the preReadyQ by priority
 * loop through the preReadyQ and load processes to readyQ
 * if any cpus are free, move process to the free cpu
 */
void fromPreReadyToReadyToCPU(void) 
{
    qsort(preReadyQ, preReadyQSize, sizeof(process*), compareProcessPointers);
    
    for (int i=0;i<preReadyQSize;i++) 
    {
        enqueueProcess(&readyQ, preReadyQ[i]);
    }
    
    preReadyQSize = 0;

    for (int i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] == NULL) 
        {
            cpu[i] = nextScheduledProcess(); 
        }
    }
}

/* move any running processes that have finished their CPU burst to waiting,
and terminate those that have finished their last CPU burst. */
void fromRunningToWaiting(void) 
{
    int i;
    
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] != NULL) 
        {
            /* if process's current (CPU) burst is finished */
            if (cpu[i]->bursts[cpu[i]->currentBurst].step == cpu[i]->bursts[cpu[i]->currentBurst].length) 
            {
                /* start process' next (I/O) burst */
                cpu[i]->currentBurst++;

                /* move process to waiting queue if it is not finished */
                if (cpu[i]->currentBurst < cpu[i]->numberOfBursts) 
                {
                    enqueueProcess(&ioWaitingQ, cpu[i]);

                    /* otherwise, terminate it (don't put it back in the queue) */
                } 
                else 
                {
                    cpu[i]->endTime = clock_cycle;
                }

                /* stop executing the process
                -since this will remove the process from the cpu immediately,
                but the process is supposed to stop running at the END of
                the current time step, we need to add 1 to the runtime */
                cpu[i] = NULL;
            }
            else if(cpu[i]->quantumRemaining == 0)
			{
                //printf("in else part\n");
                /* if process's current (CPU) burst is not finished but time quanta finished*/
				cpu[i]->quantumRemaining = tQuantum; //update the time quanta back to full
				totalContextSwitches++;
                
                enqueueProcess(&readyQ, cpu[i]); //put the process to the back of the readyQ
				cpu[i] = NULL; //free the CPU

                
                //printf("totalContextSwitches=%d\n", totalContextSwitches);
				
			}
        }
    }
}

/* increment each waiting process' current I/O burst's progress */
void incrementIOWaiting(void) 
{
    int i;
    int size = ioWaitingQ.size;
    for (i=0;i<size;i++) 
    {
        process *front = ioWaitingQ.front->data; /* get process at front */
        dequeueProcess(&ioWaitingQ); /* dequeue it */

        /* increment the current (I/O) burst's step (progress) */
        front->bursts[front->currentBurst].step++;
        enqueueProcess(&ioWaitingQ, front); /* enqueue it again */

    }
}

/* increment waiting time for each process in the ready queue */
void incrementReadyQIdle(void) 
{
    int i;
    for (i=0;i<readyQ.size;i++) 
    {
        process *front = readyQ.front->data; /* get process at front */
        dequeueProcess(&readyQ); /* dequeue it */
        front->waitingTime++; /* increment waiting time */
        enqueueProcess(&readyQ, front); /* enqueue it again */
    }
}

/* update the progress for all currently executing processes */
void incrementCPUBurst(void) 
{
    int i;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] != NULL) 
        {
            /* increment the current (CPU) burst's step (progress) */
            cpu[i]->bursts[cpu[i]->currentBurst].step++;
            cpu[i]->quantumRemaining--;
            //printf("cpu quanta= %d step= %d\n",cpus[i]->quantumRemaining, cpus[i]->bursts[cpus[i]->currentBurst].step);
            
        }
    }
}

int main(int argc, char *argv[]) 
{
    int sumOfTurnaroundTimes = 0;
    int status = 0;
    int i;

    initializeGlobals();
    //printf("ARGS=%s\n",argv[1]);
    /* getting the time quantum from user input */
    tQuantum = atoi(argv[1]);
    printf("tQantum=%d\n",tQuantum);
    

    if(argc > 2)
	{
		fprintf(stderr, "Too many time quanta\n");
		return -1;
		
	}
	else if(argc < 2)
	{
		fprintf(stderr, "Please provide the time quantum, run the program again\n");
		return -1;
		
	}

    /* read in all process data and populate processes array with the results */
    
    while ((status=readProcess(&process_frm_datafile[totalProcesses]))) 
    {
        if(status==1)
        {
            totalProcesses ++;
        } 

        if(totalProcesses > MAX_PROCESSES)
        {
            break;
        }
    }

    /* handle invalid number of processes in input */
    if (totalProcesses == 0) 
    {
        fprintf(stderr, "No processes found\n");
        return -1;
    } 
    else if (totalProcesses > MAX_PROCESSES) 
    {
        fprintf(stderr, "Too many processes provided in input. Max number of processes is %d.\n", MAX_PROCESSES);
        return -1;
    }

    /* sorting by arrival time */
    qsort(process_frm_datafile, totalProcesses, sizeof(process), compareByArrival);
    //printf("Sorted processes\n");
    //printf("tQantum=%d\n",tQuantum);
    while (1) 
    {
        enquePreReadyQ(); /* add the new arriving processes to preReadyQ */
        fromRunningToWaiting(); /* move procs that shouldn't be running */
        //printf("dddd\n");
        fromIOWaitingToPreReady(); /* move procs finished waiting to ready-Q */
        fromPreReadyToReadyToCPU(); /* move ready procs into any free cpu slots */

        incrementIOWaiting(); /* update burst progress for waiting procs */
        incrementReadyQIdle(); /* update waiting time for ready procs */
        incrementCPUBurst(); /* update burst progress for running procs */

        cpuTimeUtilized += runningProcesses();

        
        if (runningProcesses() == 0 && incomingProcesses() == 0 && ioWaitingQ.size == 0) 
        {
            break;
        }
        
        clock_cycle++;
    }

    /* compute and output performance metrics */
    for (i=0;i<totalProcesses;i++) 
    {
        sumOfTurnaroundTimes += process_frm_datafile[i].endTime - process_frm_datafile[i].arrivalTime;
        totalWaitingTime += process_frm_datafile[i].waitingTime;
    }

    printf("Average waiting time : %.2f units\n"
    "Average turnaround time : %.2f units\n"
    "Time all processes finished : %d\n"
    "Average CPU utilization : %.1f%%\n"
    "Number of context switches : %d\n",
    (totalWaitingTime / (double) totalProcesses),
    (sumOfTurnaroundTimes / (double) totalProcesses),
    clock_cycle,
    (100.0 * cpuTimeUtilized / clock_cycle),
    totalContextSwitches);

    printf("PID(s) of last process(es) to finish :");
    
    for (i=0;i<totalProcesses;i++) 
    {
        if (process_frm_datafile[i].endTime == clock_cycle) 
        {
            printf(" %d", process_frm_datafile[i].pid);
        }
    }
    printf("\n");
    return 0;
}