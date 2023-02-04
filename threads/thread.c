//Weite Fan #1006761933
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

typedef enum{//this is threads' state
	READY = -1,
	RUN = -2,
	EMPTY = -3,
        EXITED = -4,  
        SLEEP = -5,
	KILLED = -6,
}state;

/* This is the node in thread queues */

typedef struct t_node{
    Tid id;
    struct t_node * next;
}t_node;

/* This is the ready queue structure */

typedef struct ready_queue{
    t_node * head;
}ready_queue;

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
};

/* This is the thread control block */
typedef struct thread {
	state t_state;
	ucontext_t t_context;
	void * t_sp;//stack pointer
}thread;

static thread t_array[THREAD_MAX_THREADS];//a array whose elements are threads       

volatile static int t_counter;//number of run or ready threads

volatile static Tid t_current;//Tid of current thread

static ready_queue rq;//ready queue of threads

void push_q(Tid id, ready_queue *rq){//add a thread node at the  tail of ready queue list
    t_node* newnode = (t_node*)malloc(sizeof(t_node));
    assert(newnode);
    newnode->id = id;
    newnode->next = NULL;
    if(rq->head == NULL){//empty queue
        rq->head = newnode;
    }
    else{
        t_node* cur = rq->head;
        while(cur->next != NULL){//find the end of queue
            cur = cur->next;
        }
        cur->next = newnode;
    }
}

/* Pops a given thread from a thread queue */

Tid pop_q_first(ready_queue *rq){// pop the first node from ready queue when calling yield THREAD_ANY
    Tid want_id;
    if(rq->head == NULL){//empty queue
        return THREAD_NONE;
    }
    else{
        t_node* cur = rq->head;
        rq->head = cur->next;
        want_id = cur->id;
        free(cur);
        return want_id;
    }
}

Tid pop_q_want(Tid want_id, ready_queue *rq){//pop particular thread by tid form ready queue 
    Tid ret;
    if(rq->head == NULL){//empty queue
        return THREAD_NONE;
    }
    else{
        t_node* cur = rq->head;
        t_node* pre = NULL;
        while(cur != NULL){
            if(cur->id == want_id){
                if(pre == NULL){
                    rq->head = cur->next;
                    ret = cur->id;
                    free(cur);
                    return ret;
                }
                pre->next = cur->next;
                ret = cur->id;
                free(cur);
                return ret;
            }
            else{
                pre = cur;
                cur = cur->next;
            }
        }
        return THREAD_INVALID;
    }
}

void
thread_init(void)
{
    for(int i=0; i<THREAD_MAX_THREADS; ++i){
       t_array[i].t_state = EMPTY; 
    }
    t_counter = 1;
    t_current = 0;
    t_array[0].t_state = RUN;//enable main thread
    int err = getcontext(&t_array[0].t_context);
    assert(!err);
}

Tid
thread_id()
{
    return t_current;
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
    thread_main(arg); // call thread_main() function with arg
    thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *arg)
{	 
    if (t_counter >= THREAD_MAX_THREADS) {// reach maximum thread numbers
        return THREAD_NOMORE;
    }
    void* sp = malloc(THREAD_MIN_STACK);//allocate a new stack
    if(sp == NULL){
        return THREAD_NOMEMORY;//memory full
    }
    Tid t_id;
    for(int i=0; i<THREAD_MAX_THREADS; ++i){//find available thread
        if(t_array[i].t_state == EMPTY){
            getcontext(&t_array[i].t_context);
            t_id = i;
            break;
        }
    }
    t_array[t_id].t_sp = sp;
    t_array[t_id].t_state = READY;
    //initialize stack
    t_array[t_id].t_context.uc_link = NULL;
    t_array[t_id].t_context.uc_stack.ss_sp = t_array[t_id].t_sp;
    t_array[t_id].t_context.uc_stack.ss_flags = 0;
    t_array[t_id].t_context.uc_stack.ss_size = THREAD_MIN_STACK;
    sp += t_array[t_id].t_context.uc_stack.ss_size;
    sp -= 8;
    //initialize register 
    t_array[t_id].t_context.uc_mcontext.gregs[REG_RSP] = (greg_t)sp;
    t_array[t_id].t_context.uc_mcontext.gregs[REG_RIP] = (greg_t)&thread_stub;
    t_array[t_id].t_context.uc_mcontext.gregs[REG_RDI] = (greg_t)fn;
    t_array[t_id].t_context.uc_mcontext.gregs[REG_RSI] = (greg_t)arg;
    push_q(t_id,&rq);
    ++t_counter;
    return t_id;
}

Tid
thread_yield(Tid want_tid)
{
    //THREAD_SELF & THREAD_ANY
    int pop_first = 0;
    if(want_tid == THREAD_SELF){
        return t_current;
    }
    else if(want_tid == THREAD_ANY){
        if(t_array[t_current].t_state == RUN && t_counter == 1){
            return THREAD_NONE;
        }
        else{
            want_tid = pop_q_first(&rq);
            pop_first = 1;
        }
    }
    
    if(want_tid < 0 || want_tid > THREAD_MAX_THREADS){
        return THREAD_INVALID;
    }
    if(t_array[want_tid].t_state != RUN && t_array[want_tid].t_state != KILLED && t_array[want_tid].t_state != READY){
        return THREAD_INVALID;
    }
    volatile int setcontext_called = 0;
    struct ucontext_t uc;
    int err = getcontext(&uc);
    for(int i = 1; i < THREAD_MAX_THREADS; ++i){//clear all exited thread, leave id for available threads
        if (t_array[i].t_state == EXITED && i != t_current){
            t_array[i].t_state = EMPTY;
            free(t_array[i].t_sp);//clear memory space 
        }
    }
    if(t_array[t_current].t_state == KILLED){
            thread_exit(0);
    }
    if(err == 0 && setcontext_called == 0){
        setcontext_called = 1;
        
        if(t_array[t_current].t_state == RUN){
            t_array[t_current].t_state = READY;
            push_q(t_current,&rq);
        }
        t_array[t_current].t_context = uc;
        if(t_array[want_tid].t_state == READY || t_array[want_tid].t_state == RUN){
            t_array[want_tid].t_state = RUN;
        }
        t_current = want_tid;
        uc = t_array[want_tid].t_context;     
        if(pop_first == 0){
            pop_q_want(want_tid,&rq);
        }
        setcontext(&uc);
    }
    return want_tid;
}

void
thread_exit(int exit_code)
{
    if (t_counter == 1) {//this is the last thread invoking exit
        exit(0);
    }
    else {
        t_array[t_current].t_state = EXITED;
        --t_counter;
        thread_yield(THREAD_ANY);
    }
}

Tid
thread_kill(Tid tid)
{
    if (tid < 0 || tid > THREAD_MAX_THREADS || tid == t_current){
        return THREAD_INVALID;
    }
    if (t_array[tid].t_state != READY) {
        return THREAD_INVALID;
    }
    t_array[tid].t_state = KILLED;
    return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
