#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p_queue.h"

//initialize the queue
void init_queue(p_queue *pQueue)
{
	pQueue->tail = -1;
	//pQueue->allThreads = calloc(1000, sizeof(thread));
}


void push(p_queue *pQueue, int priority, thread_info *info)
{
	//check if the queue is empty
	if (pQueue->tail == -1) 
	{
		//in tail we keep the position of the last element
		pQueue->tail = 0;
		
		pQueue->allThreads[0].info = info;
		pQueue->allThreads[0].priority = priority;
		return;
	} 

	int i;

	//we move all the elements to the right
	for (i=pQueue->tail; i >= 0; i--) 
	{
		if (priority <= pQueue->allThreads[i].priority)
			break;
		pQueue->allThreads[i + 1] = pQueue->allThreads[i];
	}

	//now we insert the thread sorted by priority
	i++;
	pQueue->allThreads[i].priority = priority;
	pQueue->allThreads[i].info = info;
	
	//change the value in tail
	pQueue->tail = pQueue->tail + 1;

}

void pop(p_queue *pQueue, thread_info *info)
{
	//if the queue is empty we can't delete any element
	if (pQueue->tail == -1) 
	{
		return;
	}

	int i;

	//pass trough all threads
	for (i = 0; i <= pQueue->tail; i++) 
	{
		//if we found the searched thread 
		if (pQueue->allThreads[i].info == info) 
		{
			free(pQueue->allThreads[i].info);

			//shift all elements to the left
			for (int j = i; j <= pQueue->tail; j++) 
				pQueue->allThreads[j] = pQueue->allThreads[j + 1];
				
			break;
		}
	}

		//change the value in tail
		pQueue->tail = pQueue->tail - 1;
	
}

//return the searched thread without it being erased
thread_info *peek(p_queue *pQueue)
{
    //if the queue is empty we can't delete any element
    if (pQueue->tail == -1)
        return;

    //if the queue has only one element
    if (0 == pQueue->tail)
        return pQueue->allThreads[0].info;

    //finding the thread which is not in the state waiting and return it 
    for (int i = 0; i <= pQueue->tail; i++)
    {
        if(pQueue->allThreads[i].info->waitingIo == 0)
        {
            return pQueue->allThreads[i].info;
        }
    }

    return NULL;
}

void destroy_queue(p_queue *pQueue)
{
    //if the queue is empty
    if (pQueue->tail == -1)
        return;

    for(int i = 0; i <= pQueue->tail; i++)
    {
        free(pQueue->allThreads[i].info);
    }
}
