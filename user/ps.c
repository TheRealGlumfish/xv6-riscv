// Simple ps. Takes no arguments.

#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/procstate.h"
#include "kernel/pstat.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

uint
ndigits(uint64 x) {
  uint count = 1;
  while (x >= 10) {
    x /= 10;
    count++;
  }
  return count;
}

void
print_spaces(uint count) {
  while (count-- > 0) {
    printf(" ");
  }
}

void
print_centered(const char *s, uint width) {
  uint len = strlen(s);
  if(width > len) {
    uint pad = width - len;
    uint lpad = pad / 2;
    uint rpad = pad - lpad;
    print_spaces(lpad);
    printf("%s", s);
    print_spaces(rpad);
  } else {
    printf("%s", s);
  }
}

int
main(int argc, char *argv[]) {
  if(argc != 1) {
    printf("usage: ps\n");
    exit(1);
  }
  struct uproc uprocs[NPROC];
  int n = pstat(uprocs, NPROC);
  if(n < 0) {
    printf("error: pstat failed\n");
    exit(1);
  }
  uint pid_digits = 0;
  uint sz_digits = 0;
  uint ppid_digits = 0;
  uint state_digits = 0;
  for(int i = 0; i < n; i++) {
    pid_digits = MAX(pid_digits, ndigits(uprocs[i].pid));
    sz_digits = MAX(sz_digits, ndigits(uprocs[i].sz));
    ppid_digits = MAX(ppid_digits, ndigits(uprocs[i].ppid));
    if(uprocs[i].state == ZOMBIE) {
      state_digits = MAX(state_digits, 9 + ndigits(uprocs[i].xstate));
    }
  }
  pid_digits = MAX(pid_digits, 3);
  sz_digits = MAX(sz_digits, 2);
  ppid_digits = MAX(ppid_digits, 4);
  state_digits = MAX(state_digits, 8);
  print_centered("PID", pid_digits);
  printf("|      NAME      |");
  print_centered("STATE", state_digits);
  printf("|");
  print_centered("SZ", sz_digits);
  printf("|");
  print_centered("PPID", ppid_digits);
  printf("\n");
  for(int i = 0; i < n; i++) {
    uint pid_pad = pid_digits - ndigits(uprocs[i].pid);
    print_spaces(pid_pad);
    printf("%d|%s", uprocs[i].pid, uprocs[i].name);
    uint pad = 16 - strlen(uprocs[i].name);
    print_spaces(pad);
    printf("|");
    uint pad_state;
    switch(uprocs[i].state) {
      case SLEEPING:
        pad_state = state_digits - 8;
        printf("SLEEPING");
        break;
      case RUNNABLE:
        pad_state = state_digits - 8;
        printf("RUNNABLE");
        break;
      case RUNNING:
        pad_state = state_digits - 8;
        printf("RUNNING ");
        break;
      case ZOMBIE:
        pad_state = state_digits - 9 - ndigits(uprocs[i].xstate);
        printf("ZOMBIE (%d)", uprocs[i].xstate);
        break;
      default:
        printf("error: unknown state %d pid %d", uprocs[i].state, uprocs[i].pid);
        exit(1);
    }
    print_spaces(pad_state);
    printf("|");
    uint sz_pad = sz_digits - ndigits(uprocs[i].sz);
    print_spaces(sz_pad);
    printf("%lu|", uprocs[i].sz);
    uint ppid_pad = ppid_digits - ndigits(uprocs[i].ppid);
    print_spaces(ppid_pad);
    printf("%d\n", uprocs[i].ppid);
  }
  return exit(0);
}
