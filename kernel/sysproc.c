#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "procstate.h"
#include "pstat.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Populate buf with an array of up to sz uproc structs.
// The number of entries written to buf might be smaller
// than sz, depending on how many valid processes exist.
// On success, return the number of valid processes,
// on error, return -1.
uint64
sys_pstat(void)
{
  uint64 buf;
  int sz;

  argaddr(0, &buf);
  argint(1, &sz);
  if(sz < 0) {
    return -1;
  }
  if(sz > NPROC) {
    sz = NPROC; // clamp return size to NPROC
  }
  struct uproc uprocs[NPROC];
  int n = procstat(uprocs);
  if(n < sz) {
    sz = n; // clamp return size to valid processes
  }
  if(copyout(myproc()->pagetable, buf, (char *)uprocs, sz * sizeof(struct uproc)) < 0) {
    return -1;
  }
  return n;
}

uint64
sys_trace(void)
{
  int pid;
  int mask;
  argint(0, &pid);
  argint(1, &mask);
  return ktrace(pid, (uint)mask);
}

uint64
sys_gettrace(void)
{
  int pid;
  uint64 buf;
  int sz;

  argint(0, &pid);
  argaddr(1, &buf);
  argint(2, &sz);
  if(sz < 0) {
    return -1;
  }
  return gettrace(pid, buf, sz);
}
