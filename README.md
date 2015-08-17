# optithread

A very lightweight library for C and C++ to easily implement parallelisation.

## HOW TO COMPILE:

The POSIX Threads library (pthread) is used for creating and operating on threads.

### GCC:

#### c:
It uses the gcc extensions statement expressions and nested functions.
```bash
gcc main.c optithread.c -o test -lpthread
```

#### c++:
It uses lambda functions, therefore the c++11 standard is required
```bash
g++ main.c optithread.c -o test -lpthread -std=c++11
```

### CLANG/LLVM:
It uses the blocks extension (http://clang.llvm.org/docs/LanguageExtensions.html#blocks)
Make sure to have installed the blocksruntime library

It additionally uses statement expressions.

#### c:
```bash
clang main.c optithread.c -fblocks -o test -lpthread -lBlocksRuntime
```

#### c++:
```bash
clang++ main.c optithread.c -fblocks -o test -lpthread -lBlocksRuntime
```


## HOW TO USE:

sample c source code:

```C
#include "optilang.h"

int main() {

    // start threads
    opti_start_threads();

    // mark variable as block variable. Only needed for clang; has no effect if compiling with gcc. 
    // variables, which are referenced inside a parallel loop need to be marked as block variables. 
    // see clang blocks extension documentation for more information
    __block int a=0;
    
    // store loop identifier in variable c. The REF macro must immediately be followed by a loop macro
    REF(c)
    DO {
        int j;
        for (j=0; j<1000; j++) {
            usleep(1000);
            a++;
	    }
    } CONTINUE // let the other threads do the work. Don't wait for loop to complete.
    
    FOR (k,0,100,1) {
        do_something(a, k);
        return; // equivalent to continue in a normal loop
    } SYNC // wait for loop to complete
    
    // wait for loop identified by c to complete
    opti_wait_for(c);
    
    // tell threads to exit after completing their work
    opti_shutdown_threads();
    
    return 0;
}
```

