// Simple syscall tracing utility. Usage: strace PID [MASK]

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/syscall.h"
#include "kernel/param.h"
#include "kernel/trace.h"

uint
parse_mask(const char *s)
{
  uint mask = 0;
  uint digits = 0;

  if(*s == '\0') {
    return 0;
  }

  if(s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
  }

  while(*s != '\0') {
    if(digits >= 8) {
      return 0;
    }

    char c = *s++;
    if('0' <= c && c <= '9') {
      mask = mask*16 + (c - '0');
    } else if('a' <= c && c <= 'f') {
      mask = mask*16 + (c - 'a' + 10);
    } else if('A' <= c && c <= 'F') {
      mask = mask*16 + (c - 'A' + 10);
    } else {
      return 0;
    }

    digits++;
  }

  return mask;
}

int
main(int argc, char *argv[]) {
  if(argc < 2 || argc > 3) {
    printf("usage: strace PID [MASK]\n");
    exit(1);
  }

  int pid = atoi(argv[1]);
  uint mask = 0xFFFFFFFF; // Trace all syscalls.
  if(argc == 3) {
    if(argv[2][0] == '0' && (argv[2][1] == 'x' || argv[2][1] == 'X')) {
      mask = parse_mask(argv[2]);
    } else {
      mask = atoi(argv[2]);
    }
    if(mask == 0) {
      printf("invalid mask: %s\n", argv[2]);
      exit(1);
    }
  }

  if(trace(pid, mask) < 0) {
    printf("trace failed\n");
    exit(1);
  }

  struct syscall_event *events = malloc(sizeof(struct syscall_event) * NTRACE);
  if(events == 0) {
    printf("malloc failed\n");
    trace(pid, 0);
    exit(1);
  }

  int n;
  int backoff = 1;
  while(1) {
    n = gettrace(pid, events, NTRACE);
    if(n < 0)
      break;
    if(n > 0) {
      backoff = 1;
      for(int i = 0; i < n; i++) {
        printf("syscall %d: (%ld, %ld, %ld, %ld, %ld, %ld) = %ld\n",
               events[i].num,
               events[i].args[0], events[i].args[1], events[i].args[2],
               events[i].args[3], events[i].args[4], events[i].args[5],
               events[i].retval);
      }
    } else {
      pause(backoff);
      if(backoff < 16) {
        backoff <<= 1;
      }
    }
  }

  trace(pid, 0);
  free(events);
  return 0;
}
