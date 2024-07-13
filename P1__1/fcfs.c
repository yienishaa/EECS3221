/* 
Family Name: Abeyratne
Given Name(s): Yienisha
Student Number: 218462515
EECS Login ID (the one you use to access the red server): yienisha
YorkU email address (the one that appears in eClass): yienisha@my.yorku.ca
*/

#include "sch-helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <float.h>

void waitingToReadyMoves() ;
                                        //process data structure is in helpers.creates a queue node (to be attached to a queue) to hold a process
process processes[MAX_PROCESSES+1];   // a large structure array to hold all processes read from data file 
int numberOfProcesses= 0;              // total number of processes 

process_queue* readyQ;
process_queue* ioWaitQ;
process_queue* terminatedQ;

process* cpu[4];


int main(int argc, char *argv[])
{
    int status = 0;
    

    //We need 3 process queues, ReadyQ and IO_waitingQ, cpuQ
    readyQ = (process_queue*)malloc(sizeof(process_queue));
    ioWaitQ = (process_queue*)malloc(sizeof(process_queue));
    terminatedQ = (process_queue*)malloc(sizeof(process_queue));

    initializeProcessQueue(readyQ);
    initializeProcessQueue(ioWaitQ);
    initializeProcessQueue(terminatedQ);
    
    
    

    //Read all the processors and add it to the linked list "processes"
    while ((status=readProcess(&processes[numberOfProcesses])))  {

        if(status==1){
            numberOfProcesses ++;
        }
    }

    //sort process by arrival time
    //void qsort(void *base, size_t nitems, size_t size, int (*compar)(const void *, const void*))
    qsort(processes, numberOfProcesses, sizeof(process), compareByArrival);

    process_node* node;
    
    for(int i=0; i<numberOfProcesses; i++){

        node = createProcessNode(&processes[i]);
        enqueueProcess(readyQ, node->data);
        //printf("%d , Total bursts=%d\n",node->data->pid, node->data->numberOfBursts);

    }
    
    
    int time = 0; //milliseconds
    int currentProcessCount = 0;
    int currentBurst = 0;
    int remainingTime = 0;

    //PID | Arrival_time | CPU_burst#1 | (IO_burst#1) | CPU_burst#2 | (IO_burst#2) |...
    process_node *currentPtr = NULL;
    process_node *ioPtr = NULL;

    int CPU_idle = 0;

    while(readyQ->size > 0 || ioWaitQ->size > 0 || cpu[0]!=NULL || cpu[1]!=NULL || cpu[2]!=NULL || cpu[3]!=NULL)
    //while(time < 100)
    {
        // Check if current process finishes its CPU burst
        //printf("ReadyQ=%d IOwaitQ=%d\n",readyQ->size, ioWaitQ->size);
        //currentPtr = cpuQ->front;

        for(int i=0; i < 4; i++)
        {
            if (cpu[i] != NULL) 
            {

                //printf("CPU%d \t",i);
                currentBurst = cpu[i]->currentBurst;
                remainingTime = cpu[i]->bursts[currentBurst].length - cpu[i]->bursts[currentBurst].step;
            
                if(remainingTime == 0)
                {
                    //Finished CPU processing
                    //Send to IOwaitQ by increasing the index of current burst
                    if(currentBurst+1 <= cpu[i]->numberOfBursts)
                    {
                        cpu[i]->currentBurst++;
                        enqueueProcess(ioWaitQ, cpu[i]);
                        cpu[i] = NULL;
                    }
                    else
                    {
                        enqueueProcess(terminatedQ, cpu[i]);
                        cpu[i] = NULL;
                    }
                    
                    //printf("Enqueue to IOwait - Current burst= %d for PID=%d\n",cpu[i]->currentBurst, cpu[i]->pid);
                    
                    
                    
                }
                else
                {
                    //CPU process not finished, CPU process time step increased upto the length
                    cpu[i]->bursts[currentBurst].step++;

                    //printf("CPU%d PID=%d, Current bust index=%d, Current Step=%d, length=%d\n",i,cpuQ->front->data->pid,
                //cpuQ->front->data->currentBurst, cpuQ->front->data->bursts[currentBurst].step, cpuQ->front->data->bursts[currentBurst].length);

                    //printf("Increament step - CPU%d PID=%d, Current bust index=%d, Current Step=%d, length=%d\n",i,cpu[i]->pid,
                    //            cpu[i]->currentBurst, cpu[i]->bursts[currentBurst].step, cpu[i]->bursts[currentBurst].length);
                }
                //currentPtr = currentPtr->next;
            }
            else
            {
                //printf("CPU%d \t",i);
                //If the CPU doesnt have a working process, take from readyQ and put to CPU
                if(readyQ->size > 0)
                {
                    
                    //printf("Readyq size= %d\t",readyQ->size);
                    cpu[i] = readyQ->front->data;
                    dequeueProcess(readyQ);
                
                    //printf("CPU%d PID=%d\t",i,cpu[i]->pid);
                    //printf("Readyq size= %d\n",readyQ->size);
                    //printf("ReadyQ PID=%d\n",readyQ->front->data->pid);
                }
                else
                {
                    CPU_idle++;
                    //printf("ReadyQ is empty CPU idle\n");
                }
                
            }
            
            
        }

        //printf("-------------------------------\n");

        /*
        *IOwaitQ 
        */
       /*
        ioPtr = ioWaitQ->front;
        int s = ioWaitQ->size;
        printf("s=%d\n",s);
        for(int i=0; i<s;i++)
        {
            printf("%d\n",ioWaitQ->front->data->pid);

            ioPtr = createProcessNode(ioWaitQ->front->data);
            dequeueProcess(ioWaitQ);
            enqueueProcess(ioWaitQ, ioPtr->data);
            
            

        }
        printf("\n");
        */

        if (ioWaitQ->size > 0) 
        {
            // Check if any process finishes its IO burst
            
            int sizeIO = ioWaitQ->size;
            //printf("In the ioWaitQ, size=%d\n",sizeIO);

            for(int i=0; i < sizeIO; i++)
            {
                //currentBurst = ioPtr->data->currentBurst;
                //remainingTime = ioPtr->data->bursts[currentBurst].length - ioPtr->data->bursts[currentBurst].step;

                currentBurst = ioWaitQ->front->data->currentBurst;
                remainingTime = ioWaitQ->front->data->bursts[currentBurst].length - ioWaitQ->front->data->bursts[currentBurst].step;
            
                //printf("BEFORE: ioQ PID=%d, Current bust index=%d, Current Step=%d, length=%d\n",ioWaitQ->front->data->pid,
                //ioWaitQ->front->data->currentBurst, ioWaitQ->front->data->bursts[currentBurst].step, 
                //ioWaitQ->front->data->bursts[currentBurst].length);
                //printf("Remaining time=%d\n",remainingTime);
                //We have to dequeue from ioWait and enqueue to readyQ
                if (remainingTime == 0) 
                {
                    //increment the index to CPU burst
                    

                    if(currentBurst+1 <= ioWaitQ->front->data->numberOfBursts)
                    {
                        ioWaitQ->front->data->currentBurst++;
                        enqueueProcess(readyQ, ioWaitQ->front->data);
                        dequeueProcess(ioWaitQ);
                    }
                    else
                    {
                        enqueueProcess(terminatedQ, ioWaitQ->front->data);
                        dequeueProcess(ioWaitQ);
                    }
                    
                }
                else
                {
                    //printf("IOWait pid=%d\n", currentPtr->data->pid);
                    //increament the front ioWaitQ process
                    ioWaitQ->front->data->bursts[currentBurst].step++;
                    
                    ioPtr = createProcessNode(ioWaitQ->front->data);
                    dequeueProcess(ioWaitQ);
                    enqueueProcess(ioWaitQ, ioPtr->data);
                    
                    
                    //printf("ioQ PID=%d, Current bust index=%d, Current Step=%d, length=%d\n",ioWaitQ->front->data->pid,
                //ioWaitQ->front->data->currentBurst, ioWaitQ->front->data->bursts[currentBurst].step, 
                //ioWaitQ->front->data->bursts[currentBurst].length);
                } 
                
            }    
        }
        else
        {
            //printf("IOwaitQ is empty\n");
        }

        //printf("Increament waiting\n");
        currentPtr = readyQ->front;
        //Updating the rest
        while(currentPtr != NULL)
        {
            
            //Not yet processed so wait time increased
            currentPtr->data->waitingTime++;

            //printf("waiting increased to %d for PID=%d\n",currentPtr->data->waitingTime, currentPtr->data->pid);

            currentPtr = currentPtr->next;
        }

        time++;

    }

    //Turn around time = completion time - arrival time
    //completion time = bursts time + waiting time

    //CPU actual time = total time
    //CPU expected time = only the CPU bursts

    

    ioPtr = terminatedQ->front;
    int s = terminatedQ->size;
 

    int totalWaitTime = 0;
    
    int totalBursts = 0;
    int cpuTimes = 0;
    for(int i=0; i<s;i++)
    {
        
        totalWaitTime = totalWaitTime + terminatedQ->front->data->waitingTime;

        
        for(int j=0;j<terminatedQ->front->data->numberOfBursts; j++)
        {
            totalBursts = totalBursts + terminatedQ->front->data->bursts[j].length;
            
        }

        for(int j=0;j<terminatedQ->front->data->numberOfBursts; j = j+2)
        {
            
            cpuTimes = cpuTimes + terminatedQ->front->data->bursts[j].length;
            //printf("j=%d cpuTIme=%d\n",j,cpuTimes);
        }


        ioPtr = createProcessNode(terminatedQ->front->data);
        dequeueProcess(terminatedQ);
        enqueueProcess(terminatedQ, ioPtr->data);
        
    
    }
    //printf("cpu times=%d\n",cpuTimes);
    float avgWaitTime = totalWaitTime / terminatedQ->size;
    float avgTurnAround = ((totalBursts + totalWaitTime) / terminatedQ->size);

    float cpuUtilize = (float)cpuTimes / time * 100;
    

    printf("Average waiting time : %.2f units\n",avgWaitTime);
    printf("Average turnaround time : %.2f units\n",avgTurnAround);
    printf("Time all processes finished : %d\n",time);
    printf("Average CPU utilization : %.2f%%  \n",cpuUtilize);
    printf("Number of context switches : 0\n");
    printf("PID(s) of last process(es) to finish : %d\n",terminatedQ->back->data->pid);
        
}




