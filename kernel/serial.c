//
// Serial input and output, to the virtio serial device.
//

#include "types.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "riscv.h"
#include "defs.h"

//
// user write() system calls to the serial port go here.
// uses sleep() and virtio-serial interrupts.
//
int
serialwrite(int user_src, uint64 src, int n)
{
  // TODO: Switch to chunking for large writes
  char *buf = kalloc();
  if (buf == 0) {
    return -1;
  }
  if (n > 4096) {
    n = 4096;
  }
  if(either_copyin(buf, user_src, src, n) == -1){
    kfree(buf);
    return -1;
  }
  virtio_serial_send(buf, n);
  return n;
}

//
// user read()s from the serial port go here.
// copy (up to) a whole input line to dst.
// user_dst indicates whether dst is a user
// or kernel address.
//
int
serialread(int user_dst, uint64 dst, int n)
{
  char *buf;
  if(user_dst){
    // TODO: Switch to chunking for large writes
    if(n > 4096){
      n = 4096;
    }
    buf = kalloc();
    if(buf == 0){
      return -1;
    }
  } else {
    buf = (char*) dst;
  }
  int recv_len = virtio_serial_recv(buf, n);
  if(user_dst){
    if(either_copyout(user_dst, dst, buf, recv_len) == -1){
      kfree(buf);
      return -1;
    }
    kfree(buf);
  }
  return recv_len;
}

void
serialinit(void)
{
  virtio_serial_init();

  // connect read and write system calls
  // to serialread and serialwrite.
  devsw[SERIAL].read = serialread;
  devsw[SERIAL].write = serialwrite;
};
