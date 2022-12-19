#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "so_scheduler.h"

//structure for thread data
typedef struct ThreadInfo 
{
    tid_t id;
    unsigned int priority;
    so_handler *function;
    int timeAvailable;
    int waitingIo;
    int deviceIo;
} thread_info;

typedef struct Thread 
{
	thread_info *info;
	int priority;
} thread;

typedef struct PriorityQueue
{
    int tail;
    thread allThreads[1000];
}p_queue;

void init_queue(p_queue *pQueue);
void push(p_queue *pQueue, int priority, thread_info *info);
void pop(p_queue *pQueue, thread_info *info);
thread_info *peek(p_queue *pQueue);
void destroy_queue(p_queue *pQueue);
