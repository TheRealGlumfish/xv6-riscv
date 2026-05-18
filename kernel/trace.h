// System call event
struct syscall_event {
  int num;        // System call number
  uint64 args[6]; // Arguments
  uint64 retval;  // Return value
  uint64 pc;      // Program counter
  uint64 sp;      // Stack pointer
  uint64 kpc;     // Kernel program counter
  uint64 ksp;     // Kernel stack pointer
};

// System call trace buffer
struct syscall_tb {
  uint nread;                          // Number of events read
  uint nwrite;                         // Number of events written
  struct syscall_event events[NTRACE]; // System call event ring buffer
};
