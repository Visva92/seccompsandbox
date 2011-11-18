// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include "library.h"
#include "sandbox.h"
#include "test_runner.h"


extern "C" int my_getpid(void);
extern char my_getpid_end[];

void patch_range(char *start, char *end) {
  int maps_fd;
  CHECK_SUCCEEDS((maps_fd = open("/proc/self/maps", O_RDONLY, 0)) >= 0);
  playground::Maps maps(maps_fd);
  playground::Library library;
  library.setLibraryInfo(&maps);
  char *extra_space = NULL;
  int extra_size = 0;
  char *page_start = (char *) ((uintptr_t) start & ~(getpagesize() - 1));
  CHECK_SUCCEEDS(mprotect(page_start, end - page_start,
                          PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
  library.patchSystemCallsInRange(start, end, &extra_space, &extra_size);
  CHECK_SUCCEEDS(close(maps_fd) == 0);
}

TEST(test_patching_syscall) {
  int pid = getpid();
  CHECK(my_getpid() == pid);
  char *func = (char *) my_getpid;
  char *func_end = my_getpid_end;
  patch_range(func, func_end);
#if defined(__x86_64__)
  CHECK(func[0] == '\xe9'); // e9 XX XX XX XX   jmp X
  CHECK(func[5] == '\x90'); // 90               nop
  CHECK(func[6] == '\x90'); // 90               nop
  CHECK(func[7] == '\xc3'); // c3               ret (unmodified)
#elif defined(__i386__)
  CHECK(func[0] == '\x68'); // 68 XX XX XX XX   push $X
  CHECK(func[5] == '\xc3'); // c3               ret
  CHECK(func[6] == '\x90'); // 90               nop
  CHECK(func[7] == '\xc3'); // c3               ret (unmodified)
#else
# error Unsupported target platform
#endif
  StartSeccompSandbox();
  CHECK(my_getpid() == pid);
}
