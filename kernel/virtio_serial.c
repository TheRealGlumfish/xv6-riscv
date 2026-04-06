//
// driver for qemu's virtio serial device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -device virtio-serial-device,bus=virtio-mmio-bus.1 -device virtconsole,chardev=char0 
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "virtio.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO1 + (r)))

// virtio console virtqueues.
// 0. receiveq(port0)
// 1. transmitq(port0)
// 2. control receiveq
// 3. control transmitq
// 4. receiveq(port1)
// 5. transmitq(port1)
#define RECEIVEQ_PORT0 0
#define TRANSMITQ_PORT0 1

struct virtq {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc *desc;
  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail *avail;
  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used[2..NUM].
};

static struct serial {
  struct virtq receiveq; // receive virtqueue for port 0.
  struct virtq transmitq; // transmit virtqueue for port 0.
  struct spinlock receiveq_lock;
  struct spinlock transmitq_lock;
  uint8 receiveq_done[NUM]; // has a request been completed?
} serial;

static void
init_virtq(struct virtq *vq, uint16 vq_sel)
{
  // initialize queue.
  *R(VIRTIO_MMIO_QUEUE_SEL) = vq_sel;

  // ensure queue is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  // check maximum queue size.
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  // allocate and zero queue memory.
  vq->desc = kalloc();
  vq->avail = kalloc();
  vq->used = kalloc();
  if(!vq->desc || !vq->avail || !vq->used)
    panic("virtio serial kalloc");
  memset(vq->desc, 0, PGSIZE);
  memset(vq->avail, 0, PGSIZE);
  memset(vq->used, 0, PGSIZE);

  // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)vq->desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)vq->desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)vq->avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)vq->avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)vq->used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)vq->used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    vq->free[i] = 1;
}

void
virtio_serial_init(void)
{
  uint32 status = 0;
  
  initlock(&serial.transmitq_lock, "virtio_serial_transmitq");
  initlock(&serial.receiveq_lock, "virtio_serial_receiveq");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 3 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("could not find virtio serial");
  }

  // reset device
  *R(VIRTIO_MMIO_STATUS) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // TODO: add multiport support and verify reserved feature bits
  // negotiate features
  *R(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 0;
  uint32 features_l = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  *R(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 1;
  uint32 features_h = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  if ((features_h & (1 << (VIRTIO_F_VERSION_1 - 32))) == 0) {
    panic("virtio serial is a legacy device");
  }
  // VIRTIO_CONSOLE_F_SIZE disable size setting,
  // VIRTIO_CONSOLE_F_MULTIPORT disable multiple ports,
  // VIRTIO_CONSOLE_F_EMERG_WRITE disable emergency write.
  // VIRTIO_RING_F_INDIRECT_DESC disable indirect descriptors,
  // VIRTIO_RING_F_EVENT_IDX disable used_event.
  features_l = 0;
  features_h = (1 << (VIRTIO_F_VERSION_1 - 32));
  *R(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features_l;
  *R(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 1;
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features_h;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio serial FEATURES_OK unset");

  init_virtq(&serial.receiveq, RECEIVEQ_PORT0);
  init_virtq(&serial.transmitq, TRANSMITQ_PORT0);

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO1_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc(struct virtq *vq)
{
  for(int i = 0; i < NUM; i++){
    if(vq->free[i]){
      vq->free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(struct virtq *vq, int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(vq->free[i])
    panic("free_desc 2");
  vq->desc[i].addr = 0;
  vq->desc[i].len = 0;
  vq->desc[i].flags = 0;
  vq->desc[i].next = 0;
  vq->free[i] = 1;
  wakeup(&vq->free);
}

// send buf of len bytes to serial port 0.
// buf must be a page returned by kalloc().
// the page will be freed upon request completion.
// unlike virtio_disk_rw(), this does not wait for request
// completion, rather it queues the request and returns.
// if there is no space in the queue, it sleeps.
void
virtio_serial_send(char *buf, uint32 len) {
  acquire(&serial.transmitq_lock);

  // allocate a descriptor and point it to the buffer.
  int desc_idx = -1;
  while(1) {
    desc_idx = alloc_desc(&serial.transmitq);
    if (desc_idx != -1) {
      break;
    }
    sleep(serial.transmitq.free, &serial.transmitq_lock);
  }

  serial.transmitq.desc[desc_idx].addr = (uint64) buf;
  serial.transmitq.desc[desc_idx].len = len;
  serial.transmitq.desc[desc_idx].flags = 0;

  // tell the device the index of the descriptors.
  serial.transmitq.avail->ring[serial.transmitq.avail->idx % NUM] = desc_idx;
  __sync_synchronize();
  serial.transmitq.avail->idx += 1;
  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = TRANSMITQ_PORT0;

  release(&serial.transmitq_lock);
}

uint32
virtio_serial_recv(char *buf, uint32 len) {
  acquire(&serial.receiveq_lock);

  // allocate a descriptor and point it to the buffer.
  int desc_idx = -1;
  while(1) {
    desc_idx = alloc_desc(&serial.receiveq);
    if (desc_idx != -1) {
      break;
    }
    sleep(serial.receiveq.free, &serial.receiveq_lock);
  }

  serial.receiveq.desc[desc_idx].addr = (uint64) buf;
  serial.receiveq.desc[desc_idx].len = len;
  serial.receiveq.desc[desc_idx].flags = VRING_DESC_F_WRITE; // device writes the data

  // tell the device the index of the descriptors.
  serial.receiveq.avail->ring[serial.receiveq.avail->idx % NUM] = desc_idx;
  __sync_synchronize();
  serial.receiveq.avail->idx += 1;
  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = RECEIVEQ_PORT0;

  // sleep until the wakeup from the interrupt handler.
  // when the descriptor pointing to buf is marked used
  // by the device, meaning the request has been completed.
  while(1) {
    if (serial.receiveq_done[desc_idx]) {
      serial.receiveq_done[desc_idx] = 0;
      break;
    }
    sleep(serial.receiveq.desc + desc_idx, &serial.receiveq_lock);
  }
  uint32 recv_len = serial.receiveq.desc[desc_idx].len;
  free_desc(&serial.receiveq, desc_idx);
  release(&serial.receiveq_lock);
  return recv_len;
}

void
virtio_serial_intr()
{
  uint32 interrupt_status = *R(VIRTIO_MMIO_INTERRUPT_STATUS);
  if (interrupt_status & 0x2) {
    panic("virtio_serial_intr config change"); // configuration change event
  }
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = interrupt_status & 0x1;

  acquire(&serial.transmitq_lock);
  while(serial.transmitq.used_idx != serial.transmitq.used->idx){
    __sync_synchronize();
    int desc_idx = serial.transmitq.used->ring[serial.transmitq.used_idx % NUM].id;
    kfree((void*) serial.transmitq.desc[desc_idx].addr);
    free_desc(&serial.transmitq, desc_idx);
    serial.transmitq.used_idx += 1;
  }
  release(&serial.transmitq_lock);
  
  acquire(&serial.receiveq_lock);
  while(serial.receiveq.used_idx != serial.receiveq.used->idx){
    __sync_synchronize();
    int desc_idx = serial.receiveq.used->ring[serial.receiveq.used_idx % NUM].id;
    // fetch the length of the data received by the device.
    // this may be smaller than the requested length.
    uint32 recv_len = serial.receiveq.used->ring[serial.receiveq.used_idx % NUM].len;
    serial.receiveq.desc[desc_idx].len = recv_len; // update the length of the data received
    serial.receiveq_done[desc_idx] = 1;
    wakeup(serial.receiveq.desc + desc_idx);
    serial.receiveq.used_idx += 1;
  }
  release(&serial.receiveq_lock);
}
