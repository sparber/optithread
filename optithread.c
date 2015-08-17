/*
 * Author: Alexander Sparber
 * Year: 2013 
*/


#include "optithread.h"
#include <pthread.h>

int DONE[OPTI_NTHREADS];


struct work_counter {
    int type;
    int pending;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct work_counter * next;
};

struct work {
    struct work_counter * counter;
    FUNC_DEC(function);
    int * arg;
    struct work * next;
};

struct _work_data {
    struct work * todo_first;
    struct work * todo_last;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct work_counter * wc_first;
    struct work_counter * wc_last;
    pthread_mutex_t wc_lock;
} work_data;


pthread_t THREADS[OPTI_NTHREADS-1];


//void * exec_thread(void * data);


struct work * get_work() {
    pthread_mutex_lock(&work_data.lock);
    while (1) {
        if (work_data.todo_first != NULL) {
            struct work * w = work_data.todo_first;
            work_data.todo_first = w->next;
            if (w->next == NULL)
                work_data.todo_last = NULL;
            //else
            //    pthread_cond_signal(&work_data.cond);
            pthread_mutex_unlock(&work_data.lock);
            return w;
        }
        pthread_cond_wait(&work_data.cond, &work_data.lock);
    }
}

struct work * get_specific_work(struct work_counter * wc) {
    pthread_mutex_lock(&work_data.lock);

    struct work * current = work_data.todo_first;
    struct work * previous = NULL;
    
    while (current != NULL) {
        
        if (current->counter == wc) {
            // found specific work
        
            if (previous == NULL)
                work_data.todo_first = current->next;
            else
                previous->next = current->next;
                
            if (current->next == NULL)
                work_data.todo_last = previous;
            
            break;
        }
        previous = current;
        current = current->next;
    }
    pthread_mutex_unlock(&work_data.lock);
    
    return current;
}


void * exec_thread(void * data) {
    
    int me = *(int*)data;
    free(data);
    
    char shutdown = 0;
    
    while (shutdown == 0) {
        struct work * w = get_work();
        
        if (w->function == NULL) // shutdown function was called
			shutdown = 1;
		else {
            w->function(w->arg);
            DONE[me]++;
        }
        
        pthread_mutex_lock(&w->counter->lock);
        if (--w->counter->pending == 0)
            pthread_cond_signal(&w->counter->cond);
        pthread_mutex_unlock(&w->counter->lock);
        
        if (w->counter->pending == 0 && w->counter->type == 0)
			opti_wait_for(w->counter); // cleanup work_counter
        free(w);
    }
    
    return NULL;
}

void start_thread(int i) {
    int ret;
    int * x = (int*)malloc(sizeof(int));
    *x = i;
    ret = pthread_create(THREADS+i, NULL, exec_thread, x);
    if (ret != 0) {
        printf("failed to start a new thread\n");
        exit(ret);
    }
}


void opti_start_threads() {
    work_data.todo_first = NULL;
    work_data.todo_last = NULL;

    int ret;
    
    ret = pthread_mutex_init(&work_data.lock, NULL);
    if (ret != 0) {
        printf("failed to initialize mutex\n");
        exit(ret);
    }
    ret = pthread_cond_init(&work_data.cond, NULL);
    if (ret != 0) {
        printf("failed to initialize condition object\n");
        exit(ret);
    }
    ret = pthread_mutex_init(&work_data.wc_lock, NULL);
    if (ret != 0) {
        printf("failed to initialize mutex\n");
        exit(ret);
    }
    
    int i;
    for (i=1; i<OPTI_NTHREADS; i++) {
        start_thread(i);
    }
    for (i=0; i<OPTI_NTHREADS; i++)
        DONE[i] = 0;
}


int _wait_for(struct work_counter * wc, int in_list);


struct work_counter * get_new_work_counter() {
    struct work_counter * c = (struct work_counter*)malloc(sizeof(struct work_counter));
    
    int ret;
    ret = pthread_mutex_init(&c->lock, NULL);
    if (ret != 0) {
        printf("failed to initialize mutex\n");
        exit(ret);
    }
    ret = pthread_cond_init(&c->cond, NULL);
    if (ret != 0) {
        printf("failed to initialize condition\n");
        exit(ret);
    }
    
    return c;
}


struct work_counter * opti_call(int start, int stop, int step, FUNC_DEC(function), int sync) {
    
    struct work_counter * c = get_new_work_counter();
    
    c->type = sync;
    
    c->pending = (stop-start+step-1)/step;

    int i;
    struct work * first = NULL;
    struct work * last = NULL;
    for (i=start; i!=stop; i+=step) {
        struct work * w = (struct work *)malloc(sizeof(struct work));
        
        w->counter = c;
        w->function = function;
        w->arg = (int*)malloc(sizeof(int));
        *w->arg = i;
        w->next = NULL;
        
        if (first == NULL) {
            first = w;
            last = w;
        } else {
            last->next = w;
            last = w;
        }
    }
    
    struct work * fw = NULL;
    if (sync && first) {
        // reserve first work for calling thread
        fw = first;
        first = first->next;
    }
    if (first) {
        // let the work begin
        pthread_mutex_lock(&work_data.lock);
        if (work_data.todo_first == NULL) {
            work_data.todo_first = first;
            work_data.todo_last = last;
        } else {
            work_data.todo_last->next = first;
            work_data.todo_last = last;
        }
        //pthread_cond_signal(&work_data.cond);
        pthread_cond_broadcast(&work_data.cond);
        pthread_mutex_unlock(&work_data.lock);
    }
    if (sync) {
        while (fw != NULL) {
        
            fw->function(fw->arg);
            DONE[0]++; // wrong, if calling thread is not main thread
            
            pthread_mutex_lock(&c->lock);
            c->pending--;
            pthread_mutex_unlock(&c->lock);
            free(fw);
            
            if (c->pending == 0)
                break;
            
            fw = get_specific_work(c);
        }
        
        _wait_for(c, 0); // wait for other threads to complete
        return NULL;
    } else {
        // append work_counter to list of unsynchronised work_counters
		pthread_mutex_lock(&work_data.wc_lock);
		c->next = NULL;
		if (work_data.wc_first == NULL) {
			work_data.wc_first = c;
			work_data.wc_last = c;
		} else {
			work_data.wc_last->next = c;
			work_data.wc_last = c;
		}
		pthread_mutex_unlock(&work_data.wc_lock);
		return c;
	}
}

int _wait_for(struct work_counter * wc, int in_list) {
	if (wc == NULL)
		return 0;
	if (in_list) {
		pthread_mutex_lock(&work_data.wc_lock);
		struct work_counter * prev = NULL;
		struct work_counter * it = work_data.wc_first;
		while (it != NULL) {
			if (it == wc) {
				if (prev == NULL) {
					work_data.wc_first = wc->next;
					if (work_data.wc_first == NULL)
						work_data.wc_last = NULL;
				} else
					prev->next = wc->next;
				break;
			}
			prev = it;
			it = it->next;
		}
		pthread_mutex_unlock(&work_data.wc_lock);
		if (it == NULL)
			return 0;
	}
	
	pthread_mutex_lock(&wc->lock);
	wc->type = 1;
	while (wc->pending > 0) {
		pthread_cond_wait(&wc->cond, &wc->lock);
	}
	pthread_mutex_unlock(&wc->lock);
	
	pthread_mutex_destroy(&wc->lock);
	pthread_cond_destroy(&wc->cond);
	free(wc);
	
	return 0;
}

int opti_wait_for(struct work_counter * wc) {
	return _wait_for(wc, 1);
}

int opti_wait_for_all() {
	while (work_data.wc_first != NULL) {
		int ret;
		if ((ret = opti_wait_for(work_data.wc_first)))
			return ret;
	}
	return 0;
}

int opti_shutdown_threads() {
	int ret = opti_wait_for(opti_call(1, OPTI_NTHREADS, 1, NULL, 0));
	pthread_mutex_destroy(&work_data.lock);
	pthread_cond_destroy(&work_data.cond);
	pthread_mutex_destroy(&work_data.wc_lock);
	return ret;
}


int opti_get_done(int thread) {
    return DONE[thread];
}
