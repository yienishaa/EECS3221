/* 
Family Name: Abeyratne
Given Name(s): Yienisha
Student Number: 218462515
EECS Login ID (the one you use to access the red server): yienisha
YorkU email address (the one that appears in eClass): yienisha@my.yorku.ca
*/
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


#define TEN_MILLIS_IN_NANOS 10000000

typedef struct {
    char data;
    off_t offset;
} BufferItem;

BufferItem *buffer;

typedef struct {
    int thread;
    BufferItem *buf;
} ParametersPass;

ParametersPass *parametersIN;
ParametersPass *parametersOUT;

FILE *file_in;
FILE *file_out;
FILE *log_file;

struct timespec tim;

pthread_mutex_t mutex;

int nIN;
int nOUT;
int bufSize;
int currentINthread;
int currentOUTthread;

int insertPointer = 0;
int removePointer = 0;

sem_t empty;
sem_t full;

int nsleep() 
{
    printf("in nspeep\n");
    struct timespec tim = {0, 0};
    tim.tv_sec = 0;
    tim.tv_nsec = rand() % (TEN_MILLIS_IN_NANOS+1);

    if(nanosleep(&tim , NULL) < 0 ) 
    {
        fprintf(stderr, "Nano sleep system call failed \n");
        exit(1); //ERROR
    }
    
}



void read_byte(int thread, BufferItem *item) 
{
    printf("IN read_byte\n");
    /* Acquire mutex lock to protect read and toLog*/
    pthread_mutex_lock(&mutex);
    int fin= fileno(file_in);
    printf("fin %d\n",fin);
    //get current offset
    //off_t lseek(int fd, off_t offset, int whence);
    if ((item->offset = lseek(fin, 0, SEEK_CUR)) < 0) 
    {
        pthread_mutex_unlock(&mutex); /* Release read mutex lock */
        fprintf(stderr, "Cannot seek output file.\n");
        exit(1); //ERROR
    }
    printf("offset %d\n",item->offset);
    //read the byte
    if( read(fin, &(item->data), 1) < 1) 
    {
        printf("read_byte PT%d EOF pthread_exit(0)\n", thread);
        pthread_mutex_unlock(&mutex); /* Release read mutex lock */
        pthread_exit(0); //EOF
    }
    
    //toLog("read_byte", thread, *item, -1); //log data
    pthread_mutex_unlock(&mutex); /* Release read mutex lock */
}

void write_byte(int thread, BufferItem *item) 
{
    // Acquire mutex lock to protect write and toLog
    pthread_mutex_lock(&mutex);
    int fout = fileno(file_out);
    
    if (lseek(fout, item->offset, SEEK_SET) < 0) 
    {
        pthread_mutex_unlock(&mutex); /* Release write mutex lock */
        fprintf(stderr, "Cannot seek output file.\n");
        exit(1); //ERROR
    }
    printf("item data %d\n",item->data);
    //write the byte
    if( write(fout, &item->data, 1) < 1) 
    {
        pthread_mutex_unlock(&mutex); /* Release write mutex lock */
        fprintf(stderr, "Cannot write to output file.\n");
        exit(1); //ERROR
    }
    
    //toLog("write_byte", thread, item, -1); //log data
    pthread_mutex_unlock(&mutex); //Release write mutex lock 
}

// produce to buffer
void produce(int thread, BufferItem *item) 
{
    sem_wait(&empty); /* Acquire empty semaphore */
    
    /* Acquire mutex lock to protect buffer (insertPointer ) and toLog*/
    pthread_mutex_lock(&mutex);
    
    /* insert_item */
    buffer[insertPointer] = *item;
    
    //toLog("produce", thread, item, insertPointer); //log data
    insertPointer = (insertPointer + 1) % bufSize;
    
    /* Release mutex lock and signal full semaphore */
    pthread_mutex_unlock(&mutex);
    sem_post(&full);
}

// consume from buffer
void consume(int thread, BufferItem *item) 
{
    sem_wait(&full); // Acquire full semaphore 
    //To count the consumer threads waiting on the semaphore, you can
    //create your own counting function to call here (note that on Linux
    //sem_getvalue() may not provide you with the values you need):
    //sem_wait_full_count(); //Acquire full semaphore with counting
    
    
    // Acquire mutex lock to protect buffer (removePointer) and toLog
    pthread_mutex_lock(&mutex);
    
    // remove item 
    *item = buffer[removePointer];
    
    //toLog("consume", thread, *item, removePointer); //log data
    removePointer = (removePointer + 1) % bufSize;
    
    // Release mutex lock and signal empty semaphore 
    pthread_mutex_unlock(&mutex);
    sem_post(&empty);
    printf("end of consume\n");
}

void *producer(void *param) 
{
    ParametersPass *parameters = (ParametersPass *)param;
    printf("In producer\n");
    while(!feof(file_in))
    {
        printf("PRODUCER: while loop\n");
        //nsleep();
        printf("slept and woke up\n");
        read_byte(parameters->thread, parameters->buf);
        printf("PRODUCER: byte read\n");
        //nsleep();
        produce(parameters->thread, parameters->buf);
    }
}

void *consumer(void *param) 
{
    ParametersPass *parameters = (ParametersPass *)param;
    printf("In consumer\n");
    while(removePointer < nOUT) 
    {
        //nsleep();
        consume(parameters->thread,  parameters->buf);
        //nsleep();
        write_byte(parameters->thread,  parameters->buf);
    }
}

int main(int argc, char *argv[])
{
    /**
     * <nIN> <nOUT> <file> <copy> <bufSize> <Log>
     */

    
    nIN = atoi(argv[1]); //number of IN threads to create.
    nOUT = atoi(argv[2]); //number of OUT threads to create
    bufSize = atoi(argv[5]); //capacity, in terms of buffer slots, of the shared buffer. This should be at least 1.

    if (!(file_in = fopen(argv[3], "r"))) 
    {
        //error("ERROR OPENING READ FILE");
    }

    if (!(file_out = fopen(argv[4], "w"))) 
    {
        //error("ERROR OPENING WRITE FILE");
    } 

    if (!(log_file = fopen(argv[6], "w"))) 
    {
        // error("ERROR OPENING WRITE FILE");
    } 

    //This is the circular buffer
    buffer = (BufferItem *)malloc(sizeof(BufferItem) * bufSize);

    parametersIN = (ParametersPass *)malloc(sizeof(ParametersPass)* nIN);
    parametersOUT = (ParametersPass *)malloc(sizeof(ParametersPass) * nOUT);
    
    
    pthread_t INthread[nIN];
	pthread_t OUTthread[nOUT];
	pthread_attr_t attr;
	pthread_attr_init(&attr);

    //pthread_mutex_init(&mutex, NULL);

    sem_init(&empty, 0, bufSize);
    sem_init(&full, 0, 1);

    //creating n number of threads

    for(int i=0;i<nIN;i++)
	{
        printf("IN thread %d\n",i);
        parametersIN[i].thread = i;
		pthread_create(&INthread[i], &attr, (void *) producer, &parametersIN[i]);
	}

    for(int i=0;i<nOUT;i++)
	{
        printf("OUT thread %d\n",i);
        parametersOUT[i].thread = i;
		pthread_create(&OUTthread[i], &attr, (void *) consumer, &parametersOUT[i]);
	}

    for(int i = 0; i < nIN; i++)
	{
		pthread_join(INthread[i], NULL);

	}

	for(int i = 0; i < nOUT; i++)
	{
		pthread_join(OUTthread[i], NULL);
	}

    pthread_mutex_destroy(&mutex);

    sem_destroy(&empty);
    sem_destroy(&full);
    fclose(file_in);
	fclose(file_out);
	free(buffer);
	return 0;
	
}