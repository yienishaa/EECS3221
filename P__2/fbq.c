/*

Description: Three-level Feedback Queue FBQ (quad-core) CPU scheduling simulator.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "sch-helpers.h" /* include header file of all helper functions */

/**
 * An entering process is put in queue 0. Aprocess in queue 0 is given a time quantum of 8 milliseconds
 * If it does not finish within this time, it is moved to the tail of queue 1
 * If queue 0 is empty, the process at the head of queue 1 is given a quantum of 16 milliseconds
 * If it does not complete, it is preempted and is put into queue 2.
 * Processes in queue 2 are run on an FCFS basis but are run onlywhen queues 0 and 1 are empty.
 * 
 */

process processes[MAX_PROCESSES+1]; /* a large array to hold all processes read from data file */
                                    /* these processes are pre-sorted and ordered by arrival time */

int totalProcessCounter; /* total number of processes */
int nextProcessIndex; /* index of next process to arrive */

/* holds processes moving to the ready queue this timestep (for sorting) */
process *preReadyQ[MAX_PROCESSES];
int preReadyQSize;

process_queue readyQ_0; /* ready queue to hold all ready processes */
process_queue readyQ_1; /* ready queue to hold all ready processes */
process_queue readyQ_2; /* ready queue to hold all ready processes */
process_queue waitingQueue; /* waiting queue to hold all processes in I/O waiting */


process *cpu[NUMBER_OF_PROCESSORS]; /* processes running on each cpu */
int totalWaitingTime; /* total time processes spent waiting */
int totalContextSwitches; /* total number of preemptions */
int clock_cycle; /* time steps simulated */
int cpuTimeUtilized; /* time steps each cpu was executing */
int tQuanta_0; /* time quanta for highest priority Q*/
int tQuanta_1; /* time quanta for medium priority Q*/
int sumOfTurnaroundTimes;



#define QUEUE_0 0
#define QUEUE_1 1
#define QUEUE_2 2
#define WAITQ -1 //IO wait Q

/** Initialization functions **/

/* performs basic initialization on all global variables */
void initializeVariables(void) 
{
    int i = 0;
    for (;i<NUMBER_OF_PROCESSORS;i++) 
    {
        cpu[i] = NULL;
    }

    clock_cycle = 0;
    cpuTimeUtilized = 0;
    totalWaitingTime = 0;
    totalContextSwitches = 0;
    totalProcessCounter = 0;
    nextProcessIndex = 0;
    tQuanta_0 = 0;
    tQuanta_1 = 0;
    sumOfTurnaroundTimes = 0;

    preReadyQSize = 0;

    initializeProcessQueue(&readyQ_0);
    initializeProcessQueue(&readyQ_1);
    initializeProcessQueue(&readyQ_2);
    initializeProcessQueue(&waitingQueue);
}

/** FCFS scheduler simulation functions **/

/* compares processes pointed to by *aa and *bb by process id,
returning -1 if aa < bb, 1 if aa > bb and 0 otherwise. */
int compareProcessPointers(const void *aa, const void *bb) 
{
    process *a = *((process**) aa);
    process *b = *((process**) bb);
    if (a->pid < b->pid) return -1;
    if (a->pid > b->pid) return 1;
    assert(0); /* case should never happen, o/w ambiguity exists */
    return 0;
}

/* returns the number of processes currently executing on processors */
int runningProcesses(void) 
{
    int result = 0;
    int i;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] != NULL)
        {
            result++;
        }
    }
    return result;
}

/* returns the number of processes that have yet to arrive in the system */
int incomingProcesses(void) 
{
    return totalProcessCounter - nextProcessIndex;
}

/* simulates the CPU scheduler, fetching and dequeuing the next scheduled
process from the ready queue. it then returns a pointer to this process,
or NULL if no suitable next process exists. */
// All FCFS processers added here to Q-2
process *nextScheduledProcess(void) 
{   
    process *result = NULL;
    int size0 = readyQ_0.size;
    int size1 = readyQ_1.size;
    int size2 = readyQ_2.size;

    if(size0 > 0)
    {
        result = readyQ_0.front->data;
        result->quantumRemaining = tQuanta_0;
        dequeueProcess(&readyQ_0);
    }
    else if(size1 > 0)
    {
        result = readyQ_1.front->data;
        result->quantumRemaining = tQuanta_1;
        dequeueProcess(&readyQ_1);
    }
    else if(size2 > 0)
    {
        result = readyQ_2.front->data;
        result->quantumRemaining = 0;
        dequeueProcess(&readyQ_2);
    }

    
    return result;
}

/* enqueue newly arriving processes in the ready queue */
void enquePreReadyQ(void) 
{
    /* place newly arriving processes into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    while (nextProcessIndex < totalProcessCounter && processes[nextProcessIndex].arrivalTime <= clock_cycle) 
    {
        
        preReadyQ[preReadyQSize] = &processes[nextProcessIndex];
        preReadyQ[preReadyQSize]->quantumRemaining = tQuanta_0;
        preReadyQ[preReadyQSize]->currentQueue = QUEUE_0;
        preReadyQSize++;
        nextProcessIndex++;
        
    }
}

/* move any waiting processes that are finished their I/O bursts to ready */
void checkIObursts(void) 
{
    int i;
    int size = waitingQueue.size;

    /* place processes finished their I/O bursts into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    for (i=0;i<size;i++) 
    {
        process *front = waitingQueue.front->data; /* get process at front */
        dequeueProcess(&waitingQueue); /* dequeue it */

        //printf("Check waitQ: pid %d current busrt step %d, len %d\n",front->pid,front->bursts[front->currentBurst].step,front->bursts[front->currentBurst].length);
        assert(front->bursts[front->currentBurst].step <= front->bursts[front->currentBurst].length);

        /* if process' current (I/O) burst is finished,
        move it to the ready queue, else return it to the waiting queue */
        if (front->bursts[front->currentBurst].step == front->bursts[front->currentBurst].length) 
        {
            /* switch to next (CPU) burst and place in ready queue */
            
            front->currentBurst++;
            front->currentQueue = QUEUE_0;
            front->quantumRemaining = tQuanta_0;
            preReadyQ[preReadyQSize] = front;
            preReadyQSize++;
            
        } 
        else 
        {
            front->currentQueue = WAITQ;
            enqueueProcess(&waitingQueue, front);
        }
    }
}

/**
 * sort the preReadyQ by priority
 * loop through the preReadyQ and load processes to readyQ 0
 * if any cpus are free, move process to the free cpu
 */
void moveFromReadyQtoCPU(void) 
{
    qsort(preReadyQ, preReadyQSize, sizeof(process*), compareProcessPointers);
    
    for (int i=0;i<preReadyQSize;i++) 
    {
        preReadyQ[i]->quantumRemaining = tQuanta_0;
        preReadyQ[i]->currentQueue = QUEUE_0; //new processes enter the Queue_0
        enqueueProcess(&readyQ_0, preReadyQ[i]);
    }
    
    preReadyQSize = 0;

    /* for each idle cpu, load and begin executing
    the next scheduled process from the ready queue. */
    for (int i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] == NULL) 
        {
            cpu[i] = nextScheduledProcess();
            
        }
        
    }
}

/* move any running processes that have finished their CPU burst to waiting,
and terminate those that have finished their last CPU burst.
This is the FCFS function */
void moveRunningProcesses(int i) 
{
    
    /* if process' current (CPU) burst is finished */
    if (cpu[i]->bursts[cpu[i]->currentBurst].step == cpu[i]->bursts[cpu[i]->currentBurst].length) 
    {
        /* start process' next (I/O) burst */
        cpu[i]->currentBurst++;

        /* move process to waiting queue if it is not finished */
        if (cpu[i]->currentBurst < cpu[i]->numberOfBursts) 
        {
            cpu[i]->quantumRemaining = 0;
            cpu[i]->currentQueue = WAITQ;
            enqueueProcess(&waitingQueue, cpu[i]);
            /* otherwise, terminate it (don't put it back in the queue) */
        } 
        else 
        {
            cpu[i]->endTime = clock_cycle;
        }

        /* stop executing the process */
        cpu[i] = NULL;
    }
}   

/* move any running processes in Q-0 that have finished their time quanta to Q-1,
and move those that have finished their last CPU burst to waiting Q. 
This is the Q0 Q1 management function */
void moveFrom_Q0_to_Q1_or_Q2(int i) 
{
    /* if process' current (CPU) burst is finished */
    if (cpu[i]->bursts[cpu[i]->currentBurst].step == cpu[i]->bursts[cpu[i]->currentBurst].length) 
    {

        /* start process' next (I/O) burst */
        cpu[i]->currentBurst++;

        /* move process to waiting queue if it is not finished */
        if (cpu[i]->currentBurst < cpu[i]->numberOfBursts) 
        {
            cpu[i]->currentQueue = WAITQ;
            cpu[i]->quantumRemaining = 0;
            enqueueProcess(&waitingQueue, cpu[i]);

            /* otherwise, end the process as all busts are done */
        } 
        else if(cpu[i]->currentBurst == cpu[i]->numberOfBursts)
        {
            cpu[i]->endTime = clock_cycle;
        }
        cpu[i] = NULL;
    }
    else if(cpu[i]->currentQueue == QUEUE_0 && cpu[i]->quantumRemaining == 0)
    {
        /* The CPU busts are not done, but time quota reached, so move it to lower priority Q */
        cpu[i]->currentQueue = QUEUE_1;
        cpu[i]->quantumRemaining = tQuanta_1;
        enqueueProcess(&readyQ_1, cpu[i]);
        totalContextSwitches++;
        cpu[i] = NULL; 
        
    }
    else if(cpu[i]->currentQueue == QUEUE_1 && cpu[i]->quantumRemaining == 0)
    {
        /* The CPU busts are not done, but time quota reached, so move it to lower priority Q */
        cpu[i]->quantumRemaining = cpu[i]->bursts[cpu[i]->currentBurst].length;
        cpu[i]->currentQueue = QUEUE_2;
        enqueueProcess(&readyQ_2, cpu[i]);
        totalContextSwitches++;
        cpu[i] = NULL;
    }
}

void selectMoveRunning(void)
{
    int i;
    //printf("selectMoveRunning\n");
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        //printf("SELECTION\n");
        if (cpu[i] != NULL) 
        {
            //printf("current queue %d\n",cpus[i]->currentQueue);
            if(cpu[i]->currentQueue == QUEUE_0 || cpu[i]->currentQueue == QUEUE_1)
            {
                //printf("Selecto\n");
                moveFrom_Q0_to_Q1_or_Q2(i);
            }
            else if(cpu[i]->currentQueue == QUEUE_2)
            {
                moveRunningProcesses(i);
                //printf("Selection %d\n",cpus[i]->currentQueue);
            }
        }
        else{
            //printf("CPU[%d] is null\n",i);
        }
    }
}

/* increment each waiting process's burst's progress */
void increamentSteps(void) 
{
    int i;
    int size = waitingQueue.size;
    process *front = NULL;
    /**
     * increament bursts steps for IO wait-queue
     */
    for (i=0;i<size;i++) 
    {
        front = waitingQueue.front->data; /* get process at front */
        front->bursts[front->currentBurst].step++; /* increment the current (I/O) burst's step (progress) */
        dequeueProcess(&waitingQueue); /* dequeue it */
        enqueueProcess(&waitingQueue, front); /* enqueue it again */
    }

    /**
     * Increament waiting time on readyQ processes. 
     * These processes are queued to be sent to the next free CPU
     */
    front = NULL;
    int size0 = readyQ_0.size;
    int size1 = readyQ_1.size;
    int size2 = readyQ_2.size;
    for (i=0;i<size0;i++) 
    {
        front = readyQ_0.front->data; /* get process at front */
        front->waitingTime++; /* increment waiting time */
        dequeueProcess(&readyQ_0); /* dequeue it */
        enqueueProcess(&readyQ_0, front); /* enqueue it again */
    }

    for (i=0;i<size1;i++) 
    {
        front = readyQ_1.front->data; /* get process at front */
        front->waitingTime++; /* increment waiting time */
        dequeueProcess(&readyQ_1); /* dequeue it */
        enqueueProcess(&readyQ_1, front); /* enqueue it again */
    }

    for (i=0;i<size2;i++) 
    {
        front = readyQ_2.front->data; /* get process at front */
        front->waitingTime++; /* increment waiting time */
        dequeueProcess(&readyQ_2); /* dequeue it */
        enqueueProcess(&readyQ_2, front); /* enqueue it again */
    }

    /**
     * Increament CPU burst steps
     */
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) 
    {
        if (cpu[i] != NULL) 
        {
            cpu[i]->bursts[cpu[i]->currentBurst].step++;
            cpu[i]->quantumRemaining--;

        }
    }
}

/**
 * Turn around time = amount of time taken to complete a process, this accounts for the waiting times in Queues, CPU bursts + IOBursts
 * Totatl wait time is the time the process waits on the readyQ
 */
void timeCalc()
{
    for (int i=0; i<totalProcessCounter; i++) 
    {
        sumOfTurnaroundTimes += processes[i].endTime - processes[i].arrivalTime;
        totalWaitingTime += processes[i].waitingTime;
    }
}

/**
 * This is the waiting time of all processes / the no.of processes
 */
double avgWaitingTime()
{
    double avgWaitTime = totalWaitingTime / (double) totalProcessCounter;
    return avgWaitTime;
    
}

/**
 * This is the avg time taken for a process to complete.
 * sum the turn around times of all processes and / no.of processes
 */
double avgTurnAroundTime()
{
    double avgTurnAround = sumOfTurnaroundTimes / (double) totalProcessCounter;
    return avgTurnAround;
}

/**
 * CPU utilization is the number of CPUs which had processes running from t=0 to t=last clock cycle
 * as a percentage this is (CPU utilization count each clock cycle) / total clock cycles
 */
double cpuTimeUtil()
{
    double cpuTime = 100.0 * cpuTimeUtilized / clock_cycle;
    return cpuTime;
}

int getLastPID()
{
    int pid = 0;
    for (int i=0;i<totalProcessCounter;i++) 
    {
        if (processes[i].endTime == clock_cycle) 
        {
            pid = processes[i].pid;
        }
    }
    return pid;
}

int main(int argc, char *argv[]) 
{
    int doneReading = 0;
    int remainingProcesses = 0;

    /* read in all process data and populate processes array with the results */
    initializeVariables();

    tQuanta_0 = atoi(argv[1]);
    tQuanta_1 = atoi(argv[2]);

    while ((doneReading=readProcess(&processes[totalProcessCounter]))) 
    {
        if(doneReading==1) totalProcessCounter ++;
        if(totalProcessCounter > MAX_PROCESSES) break;
    }

    /* handle invalid number of processes in input */
    if (totalProcessCounter == 0) 
    {
        fprintf(stderr, "Error: no processes specified in input.\n");
        return -1;
    } 
    else if (totalProcessCounter > MAX_PROCESSES) 
    {
        fprintf(stderr, "Error: too many processes specified in input; they cannot number more than %d.\n", MAX_PROCESSES);
        return -1;
    }

    /* sort the processes array ascending by arrival time */
    qsort(processes, totalProcessCounter, sizeof(process), compareByArrival);

    /* run the simulation */
    while (1) 
    {
        
        enquePreReadyQ(); /* admit any newly arriving processes */
        
        selectMoveRunning(); // moveRunningProcesses(); is replaced by the selectMoveRunning move procs that shouldn't be running
        checkIObursts(); /* move procs finished waiting to ready-Q */
        moveFromReadyQtoCPU(); /* move ready procs into any free cpu slots edited the func */

        increamentSteps(); /* update burst progress for procs */

        cpuTimeUtilized += runningProcesses();

        /* calc how many more processes are yet to arrive */
        remainingProcesses = totalProcessCounter - nextProcessIndex;
        
        if (cpu[0]==NULL && cpu[1]==NULL && cpu[2]==NULL && cpu[3]==NULL && (remainingProcesses == 0) && readyQ_0.size == 0 && readyQ_1.size == 0 && readyQ_2.size == 0 && waitingQueue.size == 0) 
        
        {
            break;
        }
        
        clock_cycle++;
    }

    timeCalc(); //Calculating the sumOfTurnaroundTimes and totalWaiting time of all processes
    printf("Average waiting time : %.2f units\n", avgWaitingTime());
    printf("Average turnaround time : %.2f units\n", avgTurnAroundTime());
    printf("Time all processes finished : %d\n", clock_cycle);
    printf("Average CPU utilization : %.1f%%\n", cpuTimeUtil());
    printf("Number of context switches : %d\n",totalContextSwitches);
    printf("PID(s) of last process(es) to finish : %d\n", getLastPID());

    return 0;
}