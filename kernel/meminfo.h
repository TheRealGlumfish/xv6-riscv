// Per-process memory layout
// Ranges are exclusive [start, end),
// except the stack which is [stack_start, heap_start)
struct meminfo {
  uint64 va_text_end;         // Virtual address of end of text segment
  uint64 va_data_start;       // Virtual address of start of data segment
  uint64 va_data_end;         // Virtual address of end of data segment
  uint64 va_stack_start;      // Virtual address of start of stack segment
  uint64 va_stack_args;       // Virtual address of end of stack arguments
  uint64 va_heap_start;       // Virtual address of start of heap segment
  uint64 va_heap_end;         // Virtual address of end of heap segment
  uint64 pa_stack_start;      // Physical address of start of stack segment
  uint64 pa_stack_args;       // Physical address of end of stack arguments
  uint64 pa_stack_end;        // Physical address of end of stack segment
  uint64 pa_trapframe_start;  // Physical address of start of trapframe
  uint64 pa_trampoline_start; // Physical address of start of trampoline
  uint64 va_kstack_start;     // Virtual address of start of kernel stack
  uint64 pa_kstack_start;     // Physical address of start of kernel stack
};
