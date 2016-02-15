This is a snapshot of the ongoing work to use the Linux Seccomp mode for implementing a sandboxing solution.

This code is intended to be used in the Linux version Google Chrome as means to restrict security critical parts of the browser from making arbitrary unsafe system calls.

Ultimately, we intend to write a general purpose library that can be used by any program that has sandboxing requirements. But it will probably be still a while before we get there.


**Update (March 2015):** This project is obsolete.  It is based on v1 of Linux's Seccomp sandboxing mechanism.  Seccomp v1 whitelists a fixed set of syscalls, and the seccompsandbox codebase is basically a complicated hack to work around that to whitelist a larger set of syscalls.  It involves forwarding syscall invocations to a trusted thread to be executed.

These days, you would be better off using a sandbox based on Linux's newer Seccomp-BPF mechanism (a.k.a. Seccomp v2) instead.