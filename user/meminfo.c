// Memory layout inspection utility. Usage: meminfo PID
#include "kernel/memlayout.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/meminfo.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2) {
    printf("usage: meminfo PID\n");
    exit(1);
  }
  int pid = atoi(argv[1]);
  if(pid <= 0) {
    printf("invalid PID\n");
    exit(1);
  }

  struct meminfo info;
  if(meminfo(pid, &info) < 0) {
    printf("meminfo failed\n");
    exit(1);
  }

  uint64 text_size = info.va_text_end;
  uint64 data_size = info.va_data_end - info.va_data_start;
  uint64 stack_size = info.va_heap_start - info.va_stack_start;
  uint64 args_size = info.va_heap_start - info.va_stack_args;
  uint64 heap_size = info.va_heap_end - info.va_heap_start;

  printf("Virtual Address Space for PID %d:\n", pid);
  printf("trampoline: [%p, %p) %lu B\n", (void *)TRAMPOLINE, (void *)MAXVA,
         MAXVA - TRAMPOLINE);
  printf("trapframe:  [%p, %p) %lu B\n", (void *)TRAPFRAME, (void *)TRAMPOLINE,
         TRAMPOLINE - TRAPFRAME);
  printf("heap:       [%p, %p) %lu B\n", (void *)info.va_heap_start,
         (void *)info.va_heap_end, heap_size);
  printf("stack args: [%p, %p) %lu B (part of stack)\n",
         (void *)info.va_stack_args, (void *)info.va_heap_start, args_size);
  printf("stack:      [%p, %p) %lu B\n", (void *)info.va_stack_start,
         (void *)info.va_heap_start, stack_size);
  printf("guard page: [%p, %p) %lu B\n",
         (void *)PGROUNDUP(info.va_data_end - 1),
         (void *)PGROUNDDOWN(info.va_stack_start),
         PGROUNDDOWN(info.va_stack_start) - PGROUNDUP(info.va_data_end - 1));
  printf("data:       [%p, %p) %lu B\n", (void *)info.va_data_start,
         (void *)info.va_data_end, data_size);
  printf("text:       [%p, %p) %lu B\n", (void *)0, (void *)info.va_text_end,
         text_size);

  printf("                                                     %lu B\n",
         text_size + data_size + stack_size + heap_size);

  printf("Physical Address Space for PID %d:\n", pid);
  printf("trampoline: [%p, %p) %lu B\n", (void *)info.pa_trampoline_start,
         (void *)info.pa_trampoline_start + PGSIZE, (uint64)PGSIZE);
  printf("trapframe:  [%p, %p) %lu B\n", (void *)info.pa_trapframe_start,
         (void *)info.pa_trapframe_start + PGSIZE, (uint64)PGSIZE);
  printf("heap:       ...\n");
  printf("stack args: [%p, %p) %lu B\n", (void *)info.pa_stack_args,
         (void *)info.pa_stack_end, info.pa_stack_end - info.pa_stack_args);
  printf("stack:      [%p, %p) %lu B\n", (void *)info.pa_stack_start,
         (void *)info.pa_stack_end, info.pa_stack_end - info.pa_stack_start);
  printf("guard:      unmapped                                 0 B\n");
  printf("data:       ...\n");
  printf("text:       ...\n");

  printf("Kernel Address Space for PID %d:\n", pid);
  printf("kstack(VA): [%p, %p) %lu B\n", (void *)info.va_kstack_start,
         (void *)(info.va_kstack_start + PGSIZE), (uint64)PGSIZE);
  printf("kstack(PA): [%p, %p) %lu B\n", (void *)info.pa_kstack_start,
         (void *)(info.pa_kstack_start + PGSIZE), (uint64)PGSIZE);
  return 0;
}
