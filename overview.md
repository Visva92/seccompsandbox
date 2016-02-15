## Overview of the seccomp sandbox ##

Seccomp is a feature of the Linux kernel that is enabled in (most) contemporary Linux distributions. It restricts a thread to a small number of system calls:

  * read()
  * write()
  * exit()
  * sigreturn()

If the thread calls any other system call, the entire process gets terminated.

The latter is very desirable from a security point of view, as it means that any failure in the sandbox is very likely to just safely terminate the process.

The downside is of course that these four system calls are way too little for most applications to run successfully. We would like to be able to allow a much larger number of general system calls, but we then need to carefully inspect their arguments and decide whether we can let them pass, or if we should let them fail because they pose a security risk.

As seccomp mode is a per-thread feature, we can implement this extended filtering facility for (almost) arbitrary system calls by launching a trusted helper thread that does not enable seccomp.

Now, any time the sandboxed thread wants to make a system call other than one of the four unrestricted system calls, it serializes the request and writes it over a socketpair() that the trusted helper thread reads. The helper then inspects the request and executes it on behalf of the sandboxed thread.

This works OK for most system calls. A very small number of system calls manipulate per-thread state, and need to be emulated differently. Notable and relevant examples are thread-local storage, and POSIX signals. Fortunately, TLS is a non-issue as it gets set up once at thread creation and is then not touched any more.

Signals are more difficult and will need to be emulated in user-space. For an application that makes heavy use of signals, this could turn out to be a problem.

### Intercepting system calls ###

Unlike other sandboxing techniques, with seccomp we do not get any notification when the sandboxed thread is about to make a system call. But it is critical that we stop it from making direct system calls and we have to force it to redirect the system call through the trusted helper thread.

Any failure to do so would result in the immediate termination of the process.

We could rewrite our code to avoid making any system calls, but for any reasonably complex application that approach is infeasible.

We could also link against a specially-built copy of glibc. While technically possible, the maintenance cost of the approach is high and we would prefer to avoid doing so.

Finally, at run-time we can find all places where glibc makes system calls and rewrite the code to redirect to our wrapper functions, instead.

The latter is the approach that we chose. It turns out that even with an irregular assembly language like the Intel instruction set, it is possible to reliably find all call sites and to rewrite them.

From a security point of view, this is acceptable as any failure to find the call sites or any failure to correctly rewrite them would result in termination of the application. This is thus not a security problem.

### Dealing with memory access races ###

While many system calls pass all of their arguments in CPU registers, some data is also passed through pointers to user-controlled memory.

This is a problem, because a malicious thread could send a benign system call request to the trusted helper thread. Once the helper thread has inspected the arguments of the system call and deemed them safe, the malicious thread could now modify the memory and replace it with security critical arguments. The trusted helper would not necessarily notice this change and execute the system call in an unsafe manner.

This attack is possible, because in order to emulate many of the common system calls, the trusted helper needs to share the same address space with the sandboxed thread.

We solve this problem by writing the trusted helper in a way that it never stores temporary trusted data in memory. It treats all memory as potentially untrustworthy. This requires using extended registers (e.g. SSE) to store local variables. And it requires the trusted thread to be implemented in assembly.

In addition, for system calls that do pass critical arguments in user controlled memory, we forward the request to a trusted process (as opposed to a trusted thread). This process does not share the same address space and is thus not subject to the same race condition.

Of course, by not sharing the same address space, the trusted process often cannot meaningfully execute system calls on behalf of the sandboxed thread. We address this issue by sharing a small number of memory pages between the trusted process and the trusted thread. This memory is mapped writable in the process, and read-only in the thread.

Whenever the trusted process has inspected a complex system call and deemed it safe to execute, if generates a verified data block in this shared memory page. This data block includes a sequence number that gets updated on each access to the shared memory page. The trusted process then tells the trusted thread to execute a system call using the parameters in the data block. As the shared memory is not writable from within the context of the sandboxed thread, there is now no way for it to manipulate the system call prior to execution.

Furthermore, a small number of system calls such as clone() and exit() have to be carefully synchronized between the trusted thread and the trusted process. We use a mutex that is located in shared memory. This memory can be accessed freely by the trusted process, but is inacessible (PROT\_NONE) to the trusted thread. Whenever the trusted thread needs to operate on the mutex, it fork()s a helper process.

This process does not share any address space with its parent, except for the page that has the mutex and which was mapped MAP\_SHARED in the parent. This allows the helper to make the page writable and to access the mutex, without the parent ever gaining access to it.

### Other security considerations ###

  * It is important for the trusted thread to operate entirely on CPU registers and not to trust any memory that is accessible to the sandboxed untrusted code. This includes the stack. And in fact, we set the stack pointer to NULL in the trusted thread.

  * Any data sent or received on socketpairs() that are used for communicating with trusted parts of the sandbox has to be considered untrustworthy. As the sandboxed thread can read()/write() arbitrary file handles it can easily corrupt and/or intercept these communications.

  * Whenever we need to send or receive data that must be authenticated as originating from trusted code, we attach control messages (typically, file handles) to it. In seccomp mode the untrusted sandboxed code cannot send those message without going through the trusted code.

  * While we do not trust file handles, we do trust shared memory. This means, we have to be careful to never allow mmap(), munmap() or mprotect() to change the mappings of our shared memory, nor must they be allowed to change the mappings of any other code that gets executed by the trusted thread.

  * The trusted thread could potentially get tricked into reading data from the shared memory region while the trusted process is in the middle of changing it. In the cases where the trusted thread reads all of the data into its CPU registers prior to make a system call, this situation can be handled by a sequence number that gets updated each time the shared memory region is changed. In cases where a pointer to data in the shared memory region needs to be passed to the kernel, we acquire the secure mutex and only release it after the system call has completed.

  * Any time a new thread gets created with clone(), we have to set up a new shared memory region, and create a new trusted thread. This is difficult as we have to make sure that at no time must any untrusted code be allowed to manipulate the content of the shared memory region. There is a separate document that discusses these details.