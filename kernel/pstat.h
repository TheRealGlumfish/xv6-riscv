// Per-process state for user space (pstat system call)
struct uproc {
  enum procstate state; // Process state
  int xstate;           // Exit status to be returned to parent's wait
  int pid;              // Process ID
  int ppid;             // Parent process ID
  uint64 sz;            // Size of process memory (bytes)
  char name[16];        // Process name (debugging)
};
