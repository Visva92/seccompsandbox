## Steps to securely create a new thread ##

  * sandbox'd thread talks to trusted process and requests a new thread to be created.

  * by acquiring a mutex and suspending all further operations until the mutex has been released in the trusted thread, the trusted process makes sure that only one clone request is being processed at any given time.

  * trusted process generates write protected data in securely shared memory segment and tells trusted thread to call clone() with these parameters. This memory area has been pre-allocated earlier and is off-limits to any mmap(), munmap(), and mprotect() calls made in the untrusted code.

  * the original trusted thread calls clone() to create new nascent thread. This thread is fully privileged and typically shares all resources with the caller (i.e. the previous trusted thread), and by extension it shares all resources with the other sandboxed threads.

  * nascent thread creates socketpair() for sending requests to/from trusted thread.

  * the trusted process provided the nascent thread with the address of two shared memory pages that should be used for communicating between the trusted thread and the trusted process. The nascent thread makes the first page read-only (it can only be written by the trusted process), and the second one read-write (it is used for untrusted scratch space).

  * the nascent thread calls clone() to create the new trusted thread. The thread gets a NULL stack pointer and starts execution in the thread mainloop.

  * the nascent thread sets up thread local storage for the new untrusted sandboxed thread. This includes information such as the thread id, the identifying cookie that should be used when communicating with the trusted process, and the file handle for communicating with the trusted thread.

  * the nascent thread launches helper process that shares a copy-on-write of write version of the address space, except for any pages that are explicitly mapped as MAP\_SHARED. Most notably, this would be the page holding the secure mutex.

  * the helper process sends to the trusted process the file handles for the talking to the new trusted thread. It also sends the new thread identifier (tid) and the address of the shared memory region.

  * the helper process makes the mutex writable and releases it. Then it dies.

  * the trusted process sees that the mutex has been released, reads the data that the helper sent to it, updates the shared memory with the data that will go into the new thread's local storage, signals the completion of all these operations and then re-enters its mainloop.

  * nascent thread waits for the trusted process to signal completion of all processing for the clone() system call.

  * the nascent thread enters seccomp mode thus becoming the new untrusted sandboxed thread.

  * the new sandboxed thread returns to the code location where the clone() wrapper had originally been called. It cannot unroll the stack for this (as new threads get their own stack), but instead has to directly set all CPU registers.