// File:	mypthread.c

// Authors: Vikram Sahai Saxena(vs799), Vishwas Gowdihalli Mahalingappa(vg421)
// iLab machine tested on: -ilab1.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
#define THREAD_STACK_SIZE 5000 //thread context stack size
#define TIME_QUANTUM 5000 //time slice/quantum
#define NO_OF_PRIORITY_QUEUES 3 //Number of levels for Multi-level feedback queue
#define LOCKED 1 //Mutex Lock Value
#define UNLOCKED 1 //Mutex Unlock Value
#define SCHEDULER_NOT_INITIALIZED 0 //Scheduler is not inititalized
#define SCHEDULER_INITIALIZED 1 //Scheduler is inititalized

static uint t_id = 0;
int sched_init = SCHEDULER_NOT_INITIALIZED;
static scheduler *sch;
struct itimerval timerval_struct;//declare the timer struct
ucontext_t main_thread_context;// Main thread context
tcb main_tcb;// Main TCB

static void schedule();
static void sched_RR();
static void sched_PSJF();
static void sched_MLFQ();

void timer_init();
void block_signal(){};

//signal handler
void sighandler(int sig) {
	schedule();
}

void execute_thread(tcb *tcb_ptr, void *(*fn)(void *), void *arg);
void scheduler_init();
void ready_thread(tcb *tcb_ptr, int thread_priority, int thread_quantum_count);
tcb *select_thread();
void queue_init(queue *q);
thread_node *thread_node_init();
void enqueue(queue *q, tcb * tcb_ptr);
tcb *dequeue(queue *q);

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{
	   // YOUR CODE HERE	
		if (SCHEDULER_NOT_INITIALIZED == sched_init) { //scheduler is not initialized
                scheduler_init(); //initializes static scheduler * sch
                sched_init = SCHEDULER_INITIALIZED;
                timer_init();
        }

	   // create a Thread Control Block
	   	tcb *tcb_ptr = (tcb*) malloc(sizeof(tcb));
	   // create and initialize the context of this thread
	   	if (-1 == getcontext(&(tcb_ptr->thread_context))) {
				return -1;
		}
		tcb_ptr->thread_context.uc_link = &main_thread_context;
	   // allocate heap space for this thread's stack
	   	tcb_ptr->thread_context.uc_stack.ss_size = THREAD_STACK_SIZE;
	   	tcb_ptr->thread_context.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);

		*thread = t_id++;
		tcb_ptr->thread_id = *thread;
	   // after everything is all set, push this thread into the ready queue
		makecontext(&(tcb_ptr->thread_context), (void *)execute_thread, 3, tcb_ptr, function, arg);
		ready_thread(tcb_ptr, 0, 0);
		sch->current_thread = NULL;
		schedule();

	return 0;
};

/* current thread voluntarily surrenders its remaining runtime for other threads to use */
int mypthread_yield()
{
	// YOUR CODE HERE
	
	// change current thread's state from Running to Ready
	// save context of this thread to its thread control block
	// switch from this thread's context to the scheduler's context
	sch->current_thread->thread_status = YIELD;
    schedule();

	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr)
{
	// YOUR CODE HERE

	// preserve the return value pointer if not NULL
	// deallocate any dynamic memory allocated when starting this thread
	sch->current_thread->thread_status = TERMINATED;
    sch->current_thread->retval = value_ptr;
	swapcontext(&sch->current_thread->thread_context, &main_tcb.thread_context);
	
	return;
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr)
{
	// YOUR CODE HERE

	// wait for a specific thread to terminate
	// deallocate any dynamic memory created by the joining thread
	if (thread == (sch->current_thread->thread_id)) {
		while (TERMINATED != (sch->current_thread->thread_status)) {
		mypthread_yield();
		}
		sch->current_thread->retval = value_ptr;
		free(sch->current_thread->thread_context.uc_stack.ss_sp);
	}

	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	// YOUR CODE HERE
	
	//initialize data structures for this mutex
	if (NULL == mutex) {
		return EINVAL;
	}
	mutex->lock_value = 0;
	mutex->waiting_queue = (queue*) malloc(sizeof(queue));
	queue_init(mutex->waiting_queue);

	return 0;
};

/* aquire a mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex)
{
		// YOUR CODE HERE
	
		// use the built-in test-and-set atomic function to test the mutex
		// if the mutex is acquired successfully, return
		// if acquiring mutex fails, put the current thread on the blocked/waiting list and context switch to the scheduler thread
		if (NULL == mutex) {
			return EINVAL;
		}
		while (__sync_lock_test_and_set(&(mutex->lock_value),LOCKED)) {
			sch->current_thread->thread_status = BLOCKED;
			enqueue(mutex->waiting_queue, sch->current_thread);
			schedule();
		}
		return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex)
{
	// YOUR CODE HERE	
	
	// update the mutex's metadata to indicate it is unlocked
	// put the thread at the front of this mutex's blocked/waiting queue in to the run queue
	if (NULL == mutex) {
		return EINVAL;
	}
	__sync_lock_release(&(mutex->lock_value),UNLOCKED);
	if (0 < mutex->waiting_queue->queue_size) {
		tcb *blocked_tcb = dequeue(mutex->waiting_queue);
		ready_thread(blocked_tcb, blocked_tcb->thread_priority, blocked_tcb->thread_quantum_count);
	}
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex)
{
	// YOUR CODE HERE
	
	// deallocate dynamic memory allocated during mypthread_mutex_init
	if (NULL == mutex) {
		return EINVAL;
	}
	mutex->lock_value = 0;
	free(mutex->waiting_queue);
	return 0;
};

/* scheduler */
static void schedule()
{
	// YOUR CODE HERE
	
	// each time a timer signal occurs your library should switch in to this context
	
	// be sure to check the SCHED definition to determine which scheduling algorithm you should run
	//   i.e. RR, PSJF or MLFQ
	#ifdef RR
	sched_RR();
	#else
		#ifdef PSJF
			sched_PSJF();
		#else
			sched_MLFQ();
		#endif
	#endif
	return;

	return;
}

/* Round Robin scheduling algorithm */
static void sched_RR()
{
	// YOUR CODE HERE
	
	// Your own implementation of RR
	// (feel free to modify arguments and return types)

	//block SIGALRM
	struct sigaction action;
	sigset_t mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	action.sa_handler = block_signal;
	action.sa_mask = mask;
	action.sa_flags = 0;
	sigaction (SIGALRM, &action, NULL);

	tcb *temp = sch->current_thread;
	//move YIELD thread back to ready queue
	if (NULL != temp) {
		if (YIELD == (temp->thread_status)) {
			ready_thread(temp, 0, 0);
		}
	}

	//select current thread for execution
	if (NULL != (sch->ready_queue->front)) {
			sch->current_thread = dequeue(&(sch->ready_queue[0]));
	}
	if (NULL == (sch->current_thread)) {
		execute_thread(&main_tcb, NULL, NULL);
	}
	sch->current_thread->thread_status = RUNNING;
	(sch->current_thread->thread_quantum_count)++;
	
	//Swap contexts
	if (NULL != temp) {
		swapcontext(&(temp->thread_context), &(sch->current_thread->thread_context));
	} else {
		swapcontext(&main_thread_context, &(sch->current_thread->thread_context));
	}
	sigemptyset(&mask);
	
	return;
}

/* Preemptive PSJF (STCF) scheduling algorithm */
static void sched_PSJF()
{
	// YOUR CODE HERE

	// Your own implementation of PSJF (STCF)
	// (feel free to modify arguments and return types)

	//block SIGALRM
	struct sigaction action;
	sigset_t mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	action.sa_handler = block_signal;
	action.sa_mask = mask;
	action.sa_flags = 0;
	sigaction (SIGALRM, &action, NULL);

	tcb *temp = sch->current_thread;
	thread_node *iter = thread_node_init();

	//counter for thread that has run the least
	int min_thread_quantum_count = 0;
	
	if (NULL != temp){
		//move YIELD thread back to ready queue
		if (YIELD == temp->thread_status) {
			ready_thread(temp, 0, temp->thread_priority);
		}
	}
	//selecting thread that has run the least to be executed
	if (NULL != sch->ready_queue->front) {
			sch->current_thread = dequeue(&(sch->ready_queue[0]));
			iter->tcb_ptr = sch->current_thread;
			min_thread_quantum_count = sch->current_thread->thread_quantum_count;
			while (NULL != iter) {
				 if ((READY == (iter->tcb_ptr->thread_status))
					&& (min_thread_quantum_count >= (iter->tcb_ptr->thread_quantum_count))) {
           			min_thread_quantum_count = iter->tcb_ptr->thread_quantum_count;
            			sch->current_thread->thread_status = READY;
						sch->current_thread = iter->tcb_ptr;
       			 }
        		iter = iter->next;
			}
	}
	
	if (sch->current_thread == NULL){
		execute_thread(&main_tcb, NULL, NULL);
	}
	sch->current_thread->thread_status = RUNNING;
	(sch->current_thread->thread_quantum_count)++;
	
	//Swap context
	if(temp != NULL){
		swapcontext(&(temp->thread_context), &(sch->current_thread->thread_context));
	}
	else {
		swapcontext(&main_thread_context, &(sch->current_thread->thread_context));
	}
	sigemptyset(&mask);

	return;
}

/* Preemptive MLFQ scheduling algorithm */
/* Graduate Students Only */
static void sched_MLFQ() {
	// YOUR CODE HERE
	
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	//block SIGALRM
	struct sigaction action;
	sigset_t mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	action.sa_handler = block_signal;
	action.sa_mask = mask;
	action.sa_flags = 0;
	sigaction (SIGALRM, &action, NULL);
	tcb *temp = sch->current_thread;

	if (NULL != temp) {
		//move YIELD thread back to ready queue
		if(temp->thread_status == YIELD){
			ready_thread(temp, temp->thread_priority, temp->thread_quantum_count);
		} else {
				//set a new queue priority for a thread
				if (!(TERMINATED == temp->thread_status || BLOCKED == temp->thread_status)) {
				int new_thread_priority;
				if (NO_OF_PRIORITY_QUEUES < (temp->thread_priority)+1) {
					new_thread_priority = NO_OF_PRIORITY_QUEUES;
				} else {
					new_thread_priority = (temp->thread_priority)+1;
				}
				ready_thread(temp, new_thread_priority, temp->thread_quantum_count);
			}
		}
	}
	//select thread to be run from multilevel queue
	sch->current_thread = select_thread();
	
	if (NULL == sch->current_thread) {
		execute_thread(&main_tcb, NULL, NULL);
	}
	sch->current_thread->thread_status = RUNNING;
	(sch->current_thread->thread_quantum_count)++;
	
	//Swap context
	if(NULL != temp){
		swapcontext(&(temp->thread_context), &(sch->current_thread->thread_context));
	} else {
		swapcontext(&main_thread_context, &(sch->current_thread->thread_context));
	}
	sigemptyset(&mask);

	return;
}

// Feel free to add any other functions you need

// YOUR CODE HERE

/* initialize timer based on Time Slice/Quantum */
void timer_init() {
	signal(SIGALRM, sighandler); //run sighandler when SIGALRM signal is received
	timerval_struct.it_interval.tv_sec = 0;
	timerval_struct.it_interval.tv_usec = TIME_QUANTUM;
	timerval_struct.it_value.tv_sec = 0; //it_value if set to zero disables the alarm
	timerval_struct.it_value.tv_usec = TIME_QUANTUM; //it_value if not zero enables the alarm
	setitimer(ITIMER_REAL, &timerval_struct, NULL);//starting the itimer 
}

/* execute a thread */
void execute_thread(tcb *tcb_ptr, void *(*fn)(void *), void *arg){
	tcb_ptr->thread_status = RUNNING;
	(tcb_ptr->thread_quantum_count)++;
	sch->current_thread = tcb_ptr;
	sch->current_thread->thread_quantum_count = tcb_ptr->thread_quantum_count;
	tcb_ptr = fn(arg);
	tcb_ptr->thread_status = TERMINATED;
	schedule();
}

/* inititalize scheduler parameters */
void scheduler_init() {
	if (SCHEDULER_INITIALIZED == sched_init){
		return;
	}
	sch = malloc(sizeof(scheduler));

	//create main thread context
	getcontext(&main_thread_context);
	main_tcb.thread_context = main_thread_context;
	main_tcb.thread_context.uc_link = NULL;
	main_tcb.thread_status = READY;
	sch->main_thread = &main_tcb;
	sch->main_thread->thread_status = READY;
	sch->main_thread->thread_id = 0;
	sch->main_thread->thread_quantum_count = 0;

	//initialize ready queue
	sch->current_thread = NULL;
	sch->ready_queue = (queue*)malloc(NO_OF_PRIORITY_QUEUES*sizeof(queue));
	for (int i = 0; i < NO_OF_PRIORITY_QUEUES; i++) {
		queue_init(&(sch->ready_queue[i])); 
	}
}

/* move thread to ready queue */
void ready_thread(tcb *tcb_ptr, int thread_priority, int thread_quantum_count){
    tcb_ptr->thread_status = READY;
    tcb_ptr->thread_priority = thread_priority;
	tcb_ptr->thread_quantum_count = thread_quantum_count;
	enqueue(&(sch->ready_queue[thread_priority]), tcb_ptr);
}

/* select thread from multilevel queue*/
tcb *select_thread() {
	for (int i = 0; i < NO_OF_PRIORITY_QUEUES; i++) {
		if (NULL != (sch->ready_queue[i].front)) {
			return dequeue(&(sch->ready_queue[i]));
		}
	}
	return NULL;
}

/* initialize queue parameters */
void queue_init(queue *q) {
	q -> front = q -> rear = NULL;
	q -> queue_size = 0;
}

/* initialize */
thread_node *thread_node_init() {
	thread_node *init_node = (thread_node*)malloc(sizeof(thread_node));
	if (NULL != init_node) {
		init_node-> tcb_ptr = NULL;
		init_node-> next = NULL;
		return init_node;
	}
	return NULL;
}

/* adding TCB to queue */
void enqueue(queue *q, tcb * tcb_ptr) {
	if (0 == q->queue_size) {
		//New thread becomes the front
		thread_node * new_front_node = thread_node_init();
		q -> front = new_front_node;
		q -> rear = new_front_node;
		new_front_node -> tcb_ptr = tcb_ptr;
		(q -> queue_size)++;
	} else {
		thread_node * rear_node = q -> rear;
		thread_node * new_rear_node = thread_node_init();
		new_rear_node -> tcb_ptr = tcb_ptr;
		new_rear_node -> next = NULL;
		rear_node->next = new_rear_node;
		
		//Update rear pointer
		q -> rear = new_rear_node->next;
		(q -> queue_size)++;
	}
}

/* getting TCB from queue */
tcb *dequeue(queue *q) {
	tcb *dq_tcb = NULL;
	if (0 == q -> queue_size) {
		return dq_tcb;
	} 
	dq_tcb = (q -> front) -> tcb_ptr;
	thread_node *temp_node = (q -> front) -> next;
	free(q -> front);
	q-> front = temp_node;
	(q->queue_size)--;
    return dq_tcb;
}


