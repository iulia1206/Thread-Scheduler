# Implementation of a thread scheduler in user-space, taking the form of a shared library

This dynamic library provides a common interface to all threads that use it
and have to be planned. The exported functions:
- so_init - initializes the scheduler
- so_fork - creates a new thread in kernel-space and adds the required
	    information to the priority queue. When the thread starts, it
	    notifies the parent that it is ready for scheduling (it is ready
            to run so_handler). The other threads, which were not scheduled
	    yet, have to wait until the scheduler chooses them.
- so_exec - execution of an instruction, meaning that it decreases the amount
	    for the calling thread by one
- so_wait - marks the current thread as locked to an IO event, not allowing it
	    to be scheduled until it receives a signal from another thread
	    on the same event
- so_signal - wakes up all threads waiting on the parameter event

The complex function, schedule, based on a threads-priority-queue, always
retains the best thread at the top, except the cases when the threads wait
for IO.

The scheduler preempts the current running thread in this cases:
- a higher priority thread enters the system
- a higher priority thread is signaled
- the current thread expired
- the current thread is waiting for IO
- the current thread has no more instructions to execute

I blocked the threads using mutex and condition variables in order to be able
to signal all the other threads and analyze & schedule the threads one by one.


# Minuses:
I did not implement a function to reorder the nodes based on the Round Robin
algorithm due to the lack of time. So, when a thread was preempted and added
back to the queue, I did not implement the function that put it in its
right position according to its priority and time to arrive.

	    
