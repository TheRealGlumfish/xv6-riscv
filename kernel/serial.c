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
  printf("serialwrite: user_src: %d\n", user_src);
  printf("serialwrite: src: 0x%lx\n", src);
  printf("serialwrite: n: %d\n", n);
  // TODO: Switch to chunking for large writes
  char *buf = kalloc();
  if (buf == 0) {
    // TODO: Check whether it's better to return -1 or 0
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
  printf("serialread: user_dst: %d\n", user_dst);
  printf("serialread: dst: 0x%lx\n", dst);
  printf("serialread: n: %d\n", n);
  char *buf;
  if(user_dst){
    // TODO: Switch to chunking for large writes
    if(n > 4096){
      n = 4096;
    }
    buf = kalloc();
    if(buf == 0){
      // TODO: Check whether it's better to return -1 or 0
      return -1;
    }
  } else {
    buf = (char*) dst;
  }
  printf("serialread: send\n");
  printf("serialread: interrupt_status: %d\n", intr_get());
  int recv_len = virtio_serial_recv(buf, n);
  printf("serialread: recv_len: %d\n", recv_len);
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
