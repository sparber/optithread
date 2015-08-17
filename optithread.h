/*
 * Author: Alexander Sparber
 * Year: 2013 
*/


#ifndef OPTITHREAD_H
#define OPTITHREAD_H

#include <stdlib.h>
#include <stdio.h>


#define OPTI_NTHREADS 4

#if defined __clang__

#define FUNC_DEC(name) void (^name)(int *)

#elif defined __cplusplus

#include <functional>
#define FUNC_DEC(name) std::function<void(int*)> name
#define __block

#elif defined __GNUC__

#define FUNC_DEC(name) void (*name)(int *)
#define __block

#else

#error UNSUPPORTED COMPILER.                                               \
Define __GNUC__ if the compiler is compatible to gcc and its extensions or \
define __clang__ if the compiler is compatible to clang and its extensions

#endif



#define REF(name) \
    struct work_counter * name = 



#if defined __clang__

#define DO                                            \
({                                                    \
    int opti_sa=0, opti_so=1, opti_se=1;              \
    void (^opti_func)(int*) = ^void(int * opti_arg) { \
        free(opti_arg);

#define FOR(name,start,stop,step)                     \
({                                                    \
    int opti_sa=start, opti_so=stop, opti_se=step;    \
    void (^opti_func)(int*) = ^void(int * opti_arg) { \
        int name = *(int*)opti_arg;                   \
        free(opti_arg);

#define SYNC                                            \
    };                                                  \
    opti_call(opti_sa, opti_so, opti_se, opti_func, 1); \
});

#define CONTINUE                                        \
    };                                                  \
    opti_call(opti_sa, opti_so, opti_se, opti_func, 0); \
});


#elif defined __cplusplus

#define DO                                  \
opti_call(0,1,1,[&](int * opti_arg)->void { \
    free(opti_arg);

#define FOR(name,start,stop,step)                     \
opti_call(start,stop,step,[&](int * opti_arg)->void { \
    int name = *opti_arg;                             \
    free(opti_arg);

#define SYNC \
}, 1);
#define CONTINUE \
}, 0);


#elif defined __GNUC__

#define DO                                          \
opti_call(0,1,1,({ void opti_func(int * opti_arg) { \
    free(opti_arg);

#define FOR(name,start,stop,step)                             \
opti_call(start,stop,step,({ void opti_func(int * opti_arg) { \
    int name = *(int*)opti_arg;                               \
    free(opti_arg);

#define SYNC \
} opti_func; }), 1);
#define CONTINUE \
} opti_func; }), 0);


#endif



#define WAIT_FOR(name) \
	opti_wait_for(name);


void opti_start_threads();

struct work_counter * opti_call(int start, int stop, int step, FUNC_DEC(function), int sync);

int opti_wait_for(struct work_counter * wc);

int opti_wait_for_all();

int opti_shutdown_threads();

int opti_get_done(int thread);

#endif // OPTITHREAD_H
