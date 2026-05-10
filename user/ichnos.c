// xv6-ichnos RPC daemon. Takes no arguments.

#include "kernel/types.h"
#include "user.h"
#include "kernel/fcntl.h"
#include "kernel/procstate.h"
#include "kernel/pstat.h"
#include "kernel/meminfo.h"
#include "kernel/param.h"
#include "kernel/trace.h"

#define MAGIC 0x67676767
typedef uint16 rpc_len_t; // TODO: Verify that it never overflows.

enum req_type {
  REQ_MAGIC,
  REQ_HEARTBEAT,
  REQ_PSTAT,
  REQ_KILL,
  REQ_TRACE,
  REQ_GETTRACE,
  REQ_EXEC,
  REQ_MEMINFO,
};

struct rpc_header {
  rpc_len_t len;
  uint8 type;
} __attribute__((packed));

struct heartbeat_payload {
  uint32 heartbeat;
} __attribute__((packed));

struct heartbeat_resp {
  struct rpc_header header;
  struct heartbeat_payload payload;
} __attribute__((packed));

struct kill_payload {
  int pid;
} __attribute__((packed));

struct kill_resp {
  struct rpc_header header;
  uint8 retval;
} __attribute__((packed));

struct trace_payload {
  int pid;
  uint mask;
} __attribute__((packed));

struct trace_resp {
  struct rpc_header header;
  uint8 retval;
} __attribute__((packed));

struct gettrace_payload {
  int pid;
} __attribute__((packed));

struct exec_resp {
  struct rpc_header header;
  int pid;
} __attribute__((packed));

struct meminfo_payload {
  int pid;
} __attribute__((packed));

struct meminfo_resp {
  struct rpc_header header;
  struct meminfo info;
} __attribute__((packed));

// Reads len bytes from if into buf handling short reads.
// Calls exit(1) on error.
void
read_all(int fd, void *buf, uint32 len) {
  uint32 recv_len = 0;
  while(recv_len < len) {
    int n = read(fd, ((char*)buf) + recv_len, len - recv_len);
    if(n <= 0) {
      printf("ichnos: failed to read from serial device\n");
      exit(1);
    }
    recv_len += n;
  }
}

// Writes the buf to fd, handling short writes.
// Calls exit(1) on error.
void
write_all(int fd, void *buf, uint32 len) {
  uint32 sent_len = 0;
  while(sent_len < len) {
    int n = write(fd, ((char*)buf) + sent_len, len - sent_len);
    if(n <= 0) {
      printf("ichnos: failed to write to serial device\n");
      exit(1);
    }
    sent_len += n;
  }
}

int
main(int argc, char *argv[]) {
  if(argc != 1) {
    printf("usage: ichnos\n");
    exit(1);
  }

  int fd = open("serial", O_RDWR);
  if(fd < 0) {
    printf("ichnos: failed to open serial device\n");
    exit(1);
  }

  while (1) {
    struct rpc_header req;
    read_all(fd, &req, sizeof(req));
    switch(req.type) {
      case REQ_MAGIC: {
        if(req.len != 0) {
          printf("ichnos: invalid magic request length %u\n", req.len);
          exit(1);
        }
        struct heartbeat_resp resp = { .header = { .type = REQ_MAGIC, .len = sizeof(struct heartbeat_payload) }, .payload = { .heartbeat = MAGIC } };
        write_all(fd, &resp, sizeof(resp));
        break;
      }
      case REQ_HEARTBEAT: {
        if(req.len != 0) {
          printf("ichnos: invalid heartbeat request length %u\n", req.len);
          exit(1);
        }
        struct heartbeat_resp resp = { .header = { .type = REQ_HEARTBEAT, .len = sizeof(struct heartbeat_payload) }, .payload = { .heartbeat = (uint32)uptime() } };
        write_all(fd, &resp, sizeof(resp));
        break;
      }
      case REQ_PSTAT: {
        if(req.len != 0) {
          printf("ichnos: invalid pstat request length %u\n", req.len);
          exit(1);
        }
        // TODO: Maybe make this static (too large to be on the stack)
        struct uproc *procs = malloc(sizeof(struct uproc[NPROC]));
        if(!procs) {
          exit(1);
        }
        int nprocs = pstat(procs, NPROC);
        if(nprocs < 0) {
          printf("ichnos: pstat failed\n");
          free(procs);
          exit(1);
        }
        rpc_len_t resp_len = nprocs * sizeof(struct uproc);
        struct rpc_header resp_header = { .type = REQ_PSTAT, .len = resp_len };
        write_all(fd, &resp_header, sizeof(resp_header));
        write_all(fd, procs, resp_len);
        free(procs);
        break;
      }
      case REQ_KILL: {
        if(req.len != sizeof(struct kill_payload)) {
          printf("ichnos: invalid kill request length %u\n", req.len);
          exit(1);
        }
        struct kill_payload payload;
        read_all(fd, &payload, sizeof(payload));
        int retval = kill(payload.pid);
        struct kill_resp resp = { .header = { .type = REQ_KILL, .len = sizeof(uint8) }, .retval = !(retval < 0) };
        write_all(fd, &resp, sizeof(resp));
        break;
      }
      case REQ_TRACE: {
        if(req.len != sizeof(struct trace_payload)) {
          printf("ichnos: invalid trace request length %u\n", req.len);
          exit(1);
        }
        struct trace_payload payload;
        read_all(fd, &payload, sizeof(payload));
        int retval = trace(payload.pid, payload.mask);
        struct trace_resp resp = { .header = { .type = REQ_TRACE, .len = sizeof(uint8) }, .retval = !(retval < 0) };
        write_all(fd, &resp, sizeof(resp));
        break;
      }
      case REQ_GETTRACE: {
        if(req.len != sizeof(struct gettrace_payload)) {
          printf("ichnos: invalid gettrace request length %u\n", req.len);
          exit(1);
        }
        struct gettrace_payload payload;
        read_all(fd, &payload, sizeof(payload));
        static struct syscall_event events[NPROC];
        int n = gettrace(payload.pid, events, NTRACE);
        // TODO: Decide/finalize what kind of error handling we want here.
        if(n < 0) {
          printf("ichnos: gettrace failed for pid %d\n", payload.pid);
          n = 0;
        }
        rpc_len_t resp_len = n * sizeof(struct syscall_event);
        struct rpc_header resp_header = { .type = REQ_GETTRACE, .len = resp_len };
        write_all(fd, &resp_header, sizeof(resp_header));
        write_all(fd, events, resp_len);
        break;
      }
      case REQ_EXEC: {
        if(req.len < 1) {
          printf("ichnos: invalid exec request length %u\n", req.len);
          exit(1);
        }
        char *file = malloc(req.len);
        if(!file) {
          exit(1);
        }
        read_all(fd, file, req.len);
        int pid = fork(); // TODO: Check if fork can fail
        if(pid == 0) {
          // TODO: Add argv
          // TODO: Add wait mechanism to avoid zombies
          exec(file, (char*[]){ file, 0 });
        }
        free(file);
        struct exec_resp resp = { .header = { .type = REQ_EXEC, .len = sizeof(int) }, .pid = pid };
        write_all(fd, &resp, sizeof(resp));
        break;
      }
      case REQ_MEMINFO: {
        if(req.len != sizeof(struct meminfo_payload)) {
          printf("ichnos: invalid meminfo request length %u\n", req.len);
          exit(1);
        }
        struct meminfo_payload payload;
        read_all(fd, &payload, sizeof(payload));
        struct meminfo info;
        if(meminfo(payload.pid, &info) < 0) {
          write_all(fd, &(struct rpc_header){ .type = REQ_MEMINFO, .len = 0 }, sizeof(struct rpc_header));
        } else {
          struct meminfo_resp resp = { .header = { .type = REQ_MEMINFO, .len = sizeof(struct meminfo) }, .info = info };
          write_all(fd, &resp, sizeof(resp));
        }
        break;
      }
      default:
        printf("ichnos: unknown request type %u\n", req.type);
        exit(1);
    }
  }
}
