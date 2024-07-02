/* 
Family Name: Abeyratne
Given Name(s): Yienisha
Student Number: 218462515
EECS Login ID (the one you use to access the red server): yienisha
YorkU email address (the one that appears in eClass): yienisha@my.yorku.ca
*/

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <float.h>

typedef struct sum_calc {
	pthread_attr_t attr;	// thread attribute
	pthread_t tid;		// thread number
	char filename[25];
	double sum;			
	double dif;				
} sumStrct;

void *runner(void *param); 

int main(int argc, char *argv[])
{
        int numOfInputFiles = argc-1;
        sumStrct *sumValue = (sumStrct *) malloc(numOfInputFiles * sizeof(sumStrct));

        if(sumValue != NULL)
        {
                for(int i=0; i<numOfInputFiles;i++)
                {
                        sumValue[i].sum = 0.0;
                        sumValue[i].dif = 0.0;
                        
                        strcpy(sumValue[i].filename, argv[i+1]);
                        strcat(sumValue[i].filename,".txt");
                        
			pthread_attr_init(&(sumValue[i].attr));
			pthread_create(&sumValue[i].tid, &sumValue[i].attr, runner, &sumValue[i]);
                }
                int result;
                int remaining = numOfInputFiles;
                double parent_min,parent_max=0;
                double PMAX = DBL_MIN, PMIN = DBL_MAX;


                while(remaining>0)
                {
                        for (int i = 0; i < numOfInputFiles; i++) 
                        {
                                if (sumValue[i].tid != 0) 
                                {
                                        result = pthread_tryjoin_np(sumValue[i].tid, NULL);
                                        if (result == 0) 
                                        {
                                                parent_min = (sumValue[i].sum + sumValue[i].dif) / 2;
                                                parent_max = (sumValue[i].sum - sumValue[i].dif) / 2;

                                                if (parent_max > PMAX)
                                                {
                                                        PMAX = parent_max;
                                                }
                                                if (parent_min < PMIN)
                                                {
                                                        PMIN = parent_min;
                                                }
                                                printf("%s SUM=%lf DIF=%lf MIN=%lf MAX=%lf \n",sumValue[i].filename, sumValue[i].sum, sumValue[i].dif,parent_min, parent_max );
                                                sumValue[i].tid = 0; // Mark as joined
                                                remaining--;
                                        } 
                                        else if (result != EBUSY) 
                                        {
                                                perror("pthread_tryjoin_np failed");
                                        }
                                }
                        }
                }
                printf("MINIMUM=%lf MAXIMUM=%lf\n",PMIN,PMAX); 
        }
	free(sumValue);
        exit(0);
}


void *runner(void *param)
{
        sumStrct *sum_cal = (sumStrct *)param;
	FILE *fileptr;
        int value;
        double valdb;
        double MAX = DBL_MIN, MIN = DBL_MAX;
        
	if ((fileptr = fopen(sum_cal->filename, "r"))) 
        {
		while ((value = fscanf(fileptr, "%lf", &valdb)) != EOF ) 
                {
                        if (value == 0) 
                        {
                                fprintf(stderr, "error in file: n");
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
		fclose(fileptr);
	}
        
        sum_cal->sum = MIN + MAX;
        sum_cal->dif = MIN - MAX;
        pthread_exit(0);
}