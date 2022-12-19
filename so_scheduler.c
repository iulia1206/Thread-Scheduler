#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include "p_queue.h"
#include <pthread.h>
#include "so_scheduler.h"

typedef struct StateVariables
{
    int time;
	int nrDevices;
    int nrThreads;
    int readyVar;
    int finished;
	int timeAvailable;
}state_variables;

typedef struct ThreadData
{
    tid_t *threads;
	pthread_mutex_t lock;
	pthread_cond_t ready;
	pthread_cond_t running;
	tid_t id;
	thread_info *currentThreadInfo;
}thread_data;

typedef struct Parameters 
{
	unsigned int priority;
	so_handler *threadFunc;
}parameters;


typedef struct Scheduler 
{
	state_variables stateVariables; 
	p_queue *pQueue;
	thread_data threadData;

} scheduler;

static scheduler *sched;
int init;


// creates and initializes scheduler
int so_init(unsigned int time_quantum, unsigned int io)
{
	int ret;

	//if the scheduler is initialized
	if (init == 1)
		return -1;

	if (time_quantum <= 0 || io > SO_MAX_NUM_EVENTS) 
		return -1;

	sched = malloc(sizeof(scheduler));

	//check if the allocation was made successfuly
	if (sched == NULL)
		return -1;

	sched->stateVariables.nrThreads=0;
    sched->stateVariables.time=time_quantum;
    sched->stateVariables.nrDevices=io;

    //allocate memory 
    sched->threadData.threads = malloc(1000 * sizeof(thread_info));
    sched->pQueue = malloc(1 * sizeof(p_queue));

    //check if the allocation was made successfuly
    if (sched->threadData.threads == NULL)
        return -1;

	//check if the allocation was made successfuly
    if (sched->pQueue == NULL)
        return -1;
    
    //initialize the queue
    init_queue(sched->pQueue);

    //init mutex
    pthread_mutex_init(&sched->threadData.lock, NULL);

    //init ready condition
    pthread_cond_init(&sched->threadData.ready, NULL);

    //init running condition
    pthread_cond_init(&sched->threadData.running, NULL);

    sched->stateVariables.readyVar = 0;
    sched->stateVariables.finished = 0;
    sched->threadData.id = -1;
    sched->stateVariables.timeAvailable = -1;
    sched->threadData.currentThreadInfo = NULL;

    init = 1;
    return 0;
}

void to_running(int block)
{
	pthread_cond_broadcast(&sched->threadData.running);
	pthread_mutex_unlock(&sched->threadData.lock);

	//if the current thread is not being scheduled now
	if (block == 1)
		if (sched->threadData.id != pthread_self()) 
		{
			//waiting to be scheduled		                
			pthread_mutex_lock(&sched->threadData.lock);
			
			while (sched->threadData.id != pthread_self())
				pthread_cond_wait(&sched->threadData.running, &sched->threadData.lock);
			
			pthread_mutex_unlock(&sched->threadData.lock);
		}
}

void continue_to_next_thread()
{
	if (peek(sched->pQueue) == NULL) 
	{
		sched->threadData.currentThreadInfo->timeAvailable = sched->stateVariables.time;
	} 
	else 
	{
		sched->threadData.id = peek(sched->pQueue)->id;
		sched->threadData.currentThreadInfo = peek(sched->pQueue);
	}
}

void expired_quantum(int block)
{
	//if there is no thread in queue
		if (peek(sched->pQueue) == NULL) 
		{
			sched->threadData.currentThreadInfo->timeAvailable = sched->stateVariables.time;
			to_running(block);
		}
		//if we have found a thread with a higher or equal priority, we schedule it
		sched->threadData.currentThreadInfo->timeAvailable = sched->stateVariables.time;
		sched->threadData.id = peek(sched->pQueue)->id;
		sched->threadData.currentThreadInfo = peek(sched->pQueue);
		
	to_running(block);
}

void to_schedule_first()
{
		sched->threadData.id = peek(sched->pQueue)->id;
		sched->threadData.currentThreadInfo = peek(sched->pQueue);

		//signal the running threads that the last one arrived
		pthread_cond_broadcast(&sched->threadData.running);
		pthread_mutex_unlock(&sched->threadData.lock);
		return;
}

void schedule(int block)
{
	//there is nothing left to schedule(empty queue)
    if (sched->pQueue->tail == -1)
        return;

	pthread_mutex_lock(&sched->threadData.lock);

	//first scheduling
	if (sched->threadData.id == -1) 
	{
		to_schedule_first();
		return;
	}

	//if there's nothing  to schedule
	if (sched->stateVariables.finished == 1) 
	{
		sched->stateVariables.finished = 0;
		
		//if the time quantum expired
		if (sched->stateVariables.timeAvailable == 0)
			expired_quantum(block);
		else
			continue_to_next_thread();

	} else 
		//if the thread is waiting for an io, it is preempted
		if (sched->threadData.currentThreadInfo->waitingIo == 1) 
		{
			sched->threadData.currentThreadInfo->timeAvailable = sched->stateVariables.time;

			sched->threadData.id = peek(sched->pQueue)->id;
			sched->threadData.currentThreadInfo = peek(sched->pQueue);
		} else 
			//quantum expired
			if (sched->threadData.currentThreadInfo->timeAvailable == 0) 
			{
				//find a thread with o higher priority or reset the quantum 
				expired_quantum(block);
				
			} else {
				// if the time quantum did not expire, we try to find 
				//a thread with a higher priority 
				continue_to_next_thread();
			}

	to_running(block);
}

void *start_thread(void *p)
{
	parameters params = *(parameters*) p;

    //ready to be scheduled
    pthread_mutex_lock(&sched->threadData.lock);
    sched->stateVariables.readyVar = 1;
    pthread_cond_signal(&sched->threadData.ready);
	pthread_mutex_unlock(&sched->threadData.lock);

	//lets the right thread to pass and blocks the others
    pthread_mutex_lock(&sched->threadData.lock);

    while (sched->threadData.id != pthread_self())
		pthread_cond_wait(&sched->threadData.running, &sched->threadData.lock);
	
    pthread_mutex_unlock(&sched->threadData.lock);

	//calls handler
    params.threadFunc(params.priority);

	//the thread is scheduled
    sched->stateVariables.finished = 1;
    pop(sched->pQueue, sched->threadData.currentThreadInfo);
    
    schedule(0);
    return NULL; 
}

// creates a new thread and runs it according to the scheduler
tid_t so_fork(so_handler *func, unsigned int priority)
{
	if (func == NULL || priority > SO_MAX_PRIO)
		return INVALID_TID;

	//initialize new thread
	thread_info *newThread = malloc(sizeof(thread_info));

	if (newThread == NULL)
		return INVALID_TID;

	newThread->priority = priority;
	newThread->function = func;
	newThread->timeAvailable = sched->stateVariables.time;
	newThread->waitingIo = 0;
	newThread->deviceIo = -1;

	tid_t tId;
	parameters p;

	p.priority = priority;
	p.threadFunc = func;

	pthread_create(&tId, NULL, &start_thread, (void *) &p);

	newThread->id = tId;
	push(sched->pQueue, priority, newThread);

	sched->threadData.threads[sched->stateVariables.nrThreads] = tId;
	sched->stateVariables.nrThreads++;

	//it's getting ready for state ready
	pthread_mutex_lock(&sched->threadData.lock);
	while (sched->stateVariables.readyVar == 0)
		pthread_cond_wait(&sched->threadData.ready, &sched->threadData.lock);
	sched->stateVariables.readyVar = 0;
	pthread_mutex_unlock(&sched->threadData.lock);

	for (int i = 0; i < sched->stateVariables.nrThreads; i++) 
	{
		if (sched->threadData.threads[i] == pthread_self() == 1)
		{
			sched->threadData.currentThreadInfo->timeAvailable--;
			break;
		}
	}

	schedule(1);

	return tId;
}



//waits for an IO device
int so_wait(unsigned int io)
{
	sched->threadData.currentThreadInfo->timeAvailable--;

	if (io < 0 || io >= sched->stateVariables.nrDevices) {
		schedule(1);
		return -1;
	}

	sched->threadData.currentThreadInfo->waitingIo = 1;
	sched->threadData.currentThreadInfo->deviceIo = io;

	schedule(1);

	return 0;
}


//signals an IO device
int so_signal(unsigned int io)
{
	sched->threadData.currentThreadInfo->timeAvailable--;

	if (io < 0 || io >= sched->stateVariables.nrDevices) 
	{
		schedule(1);
		return -1;
	}

	int waitingThreads = 0;

	//check if there are any threads left
	if (sched->pQueue->tail == -1)
		{
			schedule(1);
			return waitingThreads;
		}

	int i;
	
	//find the number of threads that are waiting for a certain event
	for (i = 0; i <= sched->pQueue->tail; i++) 
	{
		if (sched->pQueue->allThreads[i].info->waitingIo == 1 && \
		sched->pQueue->allThreads[i].info->deviceIo == io) 
		{
			waitingThreads++;
			sched->pQueue->allThreads[i].info->waitingIo = 0;
		}
	}

	schedule(1);
	return waitingThreads;
}

//after one execution, timeAvaiable decreases
void so_exec(void)
{
	sched->threadData.currentThreadInfo->timeAvailable--;

	schedule(1);
}


//destroys the scheduler
void so_end()
{
    if ( init == 0)
		return;

    //waits for all threads
    for (int i = 0; i < sched->stateVariables.nrThreads; i++)
		pthread_join(sched->threadData.threads[i], NULL);

    pthread_mutex_destroy(&sched->threadData.lock);
	pthread_cond_destroy(&sched->threadData.ready);
	pthread_cond_destroy(&sched->threadData.running);

    //frees all the alocated memory
    free(sched->threadData.threads);

    destroy_queue(sched->pQueue);
    free(sched->pQueue);

    free(sched);

    init = 0;
    return;
}
