/* 
Family Name: Abeyratne
Given Name(s): Yienisha
Student Number: 218462515
EECS Login ID (the one you use to access the red server): yienisha
YorkU email address (the one that appears in eClass): yienisha@my.yorku.ca
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include<sys/shm.h>
#include <float.h>

#define BUFFER_SIZE 25
#define READ_END 0
#define WRITE_END 1

double sum,diff; /* this data is shared by the thread(s) */
double PMAX = DBL_MIN, PMIN = DBL_MAX;

typedef struct {
    pid_t pid;
    char filename[BUFFER_SIZE];
} ProcessInfo;

int main(int argc, char *argv[])
{
        char write_msg[BUFFER_SIZE];
        char read_msg[BUFFER_SIZE];
        
        int numOfInputFiles = argc - 1;
        int fileOpenedCounter = 0;
        int fd[numOfInputFiles][2]; //holds the read and write ends of the pipe

        ProcessInfo processes[numOfInputFiles];
        
        FILE *filePtr[numOfInputFiles];
        pid_t pid[numOfInputFiles];
        int value;
        double valdb;
        double MAX = DBL_MIN, MIN = DBL_MAX, SUM = 0, DIF = 0;
        
        int x =0;
        sum=0;
        diff=0;

        //printf("%d: MAIN PROCESS %d\n",getpid(),pid);

        for(fileOpenedCounter=0; fileOpenedCounter<numOfInputFiles; fileOpenedCounter++)
        {
                if (pipe(fd[fileOpenedCounter]) == -1) 
                {
                        fprintf(stderr,"Pipe failed");
                        return 1;
                }
                      
                /*fork returns 0 in the child process and a non-zero value in the parent process this non-zero value is the child process' PID.*/
                pid[fileOpenedCounter] = fork(); 
                
                //printf("%d: AFTER fork() the created child PID=%d\n",getpid(),pid[fileOpenedCounter]);
                
                if (pid[fileOpenedCounter] < 0) 
                { 
                        fprintf(stderr, "Fork Failed");
                        printf("errror\n");
                        return 1;
                }

                if(pid[fileOpenedCounter]>0)
                {
                        /*Parent process*/
                        //printf("%d: IN PARENT PROCESS %d\n",getpid(),pid[fileOpenedCounter]);
                        strcpy(write_msg, argv[fileOpenedCounter+1]);
                        strcat(write_msg,".txt");
                        write(fd[fileOpenedCounter][WRITE_END], write_msg, strlen(write_msg)+1);
                        processes[fileOpenedCounter].pid = pid[fileOpenedCounter];
                        strncpy(processes[fileOpenedCounter].filename, argv[fileOpenedCounter+1], BUFFER_SIZE);
                }
                else
                {
                        /*Child process*/
                        //printf("%d: IN CHILD PROCESS %d\n",getpid(),pid[fileOpenedCounter]);
                        read(fd[fileOpenedCounter][READ_END], read_msg, BUFFER_SIZE);
                        filePtr[fileOpenedCounter] = fopen(read_msg, "r");
                        
                        while ((value = fscanf(filePtr[fileOpenedCounter], "%lf", &valdb)) != EOF ) 
                        {
                                if (value == 0) 
                                {
                                        fprintf(stderr, "error in file: %s\n", argv[fileOpenedCounter+1]);
                                        break;
                                }
                                if(valdb>MAX)
                                {
                                        MAX = valdb;
                                }
                                else if(valdb < MIN)
                                {
                                        MIN = valdb;
                                }
                                
                        }
                        SUM = MIN + MAX;
                        DIF = MIN - MAX;
                        
                        sprintf(write_msg, "%lf %lf", SUM, DIF); 
                        write(fd[fileOpenedCounter][WRITE_END], write_msg, strlen(write_msg)+1);
                        
                        exit(0);
                }           
        }

        int status;
        pid_t wpid;
        char fileTOPrint[BUFFER_SIZE];
        double parent_min,parent_max=0;

        //printf("%d: END OF LOOP %d\n",getpid(),pid[fileOpenedCounter]);

        for(int i = 0; i < numOfInputFiles; i++)
        {
                wpid = wait(&status);
                if (wpid == -1)
                {
                        perror("wait");
                        exit(EXIT_FAILURE);
                }
                //printf("%d: FIRST PROCESS TO FINISH %d\n",getpid(),wpid);
                for (int j = 0; j < numOfInputFiles; j++)
                {
                        if (processes[j].pid == wpid)
                        {
                                close(fd[j][WRITE_END]); // Parent closes the write end
                                if (read(fd[j][READ_END], fileTOPrint, BUFFER_SIZE) > 0)
                                {
                                        sscanf(fileTOPrint, "%lf %lf", &sum, &diff);
                                        parent_min = (sum + diff) / 2;
                                        parent_max = (sum - diff) / 2;

                                        if (parent_max > PMAX)
                                        {
                                        PMAX = parent_max;
                                        }
                                        if (parent_min < PMIN)
                                        {
                                        PMIN = parent_min;
                                        }
                                        printf("%s SUM=%lf DIF=%lf MIN=%lf MAX=%lf \n", processes[j].filename, sum, diff, parent_min, parent_max);
                                }
                                close(fd[j][READ_END]);
                                break;
                        }
                }
        }
        printf("MINIMUM=%lf MAXIMUM=%lf\n",PMIN,PMAX);   
}

       
                
        

        


