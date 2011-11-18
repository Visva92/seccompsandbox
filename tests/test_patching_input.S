// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>


        // This performs a system call directly so that we can test
        // patching this instruction sequence.

        .global my_getpid
        .global my_getpid_end
my_getpid:
        mov $__NR_getpid, %eax
#if defined(__x86_64__)
        syscall
#elif defined(__i386__)
        int $0x80
#else
# error Unsupported target platform
#endif
        ret
my_getpid_end:

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
