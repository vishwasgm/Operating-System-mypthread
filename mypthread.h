// File:	mypthread.h

// Authors: Vikram Sahai Saxena(vs799), Vishwas Gowdihalli Mahalingappa(vg421)
// iLab machine tested on: -ilab1.cs.rutgers.edu

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* in order to use the built-in Linux pthread library as a control for benchmarking, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <ucontext.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

typedef uint mypthread_t;

	/* add important states in a thread control block */
typedef enum thread_states {
	READY, //thread ready for being scheduled
	RUNNING, //thread is running
	BLOCKED, //thread is blocked/waiting to be scheduled
	TERMINATED, //thread is terminated
	YIELD //yield thread to another thread
} thread_states;

typedef struct threadControlBlock
{
	// YOUR CODE HERE	
	
	// thread Id
	// thread status

	// thread context
	// thread stack
	// thread priority
	// And more ...

	//thread Id
	mypthread_t thread_id;

	//thread status
	thread_states thread_status;

	//thread context
	ucontext_t thread_context;

	//thread priority
	int thread_priority;

	//count for number of times thread is run
	int thread_quantum_count;

	//return value for thread
	void *retval;

} tcb;

/* queue node struct definition */
typedef struct thread_node {
	tcb *tcb_ptr;
	struct thread_node *next;
	
} thread_node;

/* queue node struct definition */
typedef struct queue{
	
	thread_node *front;
	thread_node *rear;
	int queue_size;
	
} queue;

/* mutex struct definition */
typedef struct mypthread_mutex_t
{

	// YOUR CODE HERE

	//thread lock value
	int lock_value;

	//waiting queue
	queue *waiting_queue;
} mypthread_mutex_t;


// Feel free to add your own auxiliary data structures (linked list or queue etc...)

/* scheduler struct definition */
typedef struct scheduler {
	queue *ready_queue; //ready queue
	tcb *main_thread; //main thread tcb pointer
	tcb *current_thread; //current thread tcb pointer
} scheduler;

/* Function Declarations: */

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* current thread voluntarily surrenders its remaining runtime for other threads to use */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initialize a mutex */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire a mutex (lock) */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release a mutex (unlock) */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy a mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
