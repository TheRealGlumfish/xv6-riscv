#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

//
// Tests the xv6 virtio serial driver. It is intended to be used with
// the Python test runner test-xv6.py. serialtest <payload> writes
// <payload> to the serial port. serialtest writef <file> writes
// the contents of <file> to the serial port. serialtest loop <n>
// reads <n> bytes from the serial port and echoes them back.
//

static void
usage(void)
{
  printf("usage: serialtest {write PAYLOAD | writef FILE | loop N}\n");
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    usage();
    exit(1);
  }

  int fd = open("serial", O_RDWR);
  if(fd < 0){
    printf("serialtest: open failed\n");
    exit(1);
  }

  if(strcmp(argv[1], "write") == 0){
    if(argc != 3){
      usage();
      close(fd);
      exit(1);
    }
    int remaining = strlen(argv[2]);
    char *p = argv[2];
    while(remaining > 0){
      int n = write(fd, p, remaining);
      if(n <= 0){
        printf("serialtest: write failed\n");
        close(fd);
        exit(1);
      }
      p += n;
      remaining -= n;
    }
  } else if(strcmp(argv[1], "writef") == 0){
    if(argc != 3){
      usage();
      close(fd);
      exit(1);
    }
    int payload_fd = open(argv[2], O_RDONLY);
    if(payload_fd < 0){
      printf("serialtest: open payload %s failed\n", argv[2]);
      close(fd);
      exit(1);
    }
    struct stat payload_stat;
    if(fstat(payload_fd, &payload_stat) < 0){
      printf("serialtest: fstat payload %s failed\n", argv[2]);
      close(payload_fd);
      close(fd);
      exit(1);
    }
    int psz = (int) payload_stat.size;
    if(psz == 0){
      close(payload_fd);
      close(fd);
      exit(0);
    }
    char *payload = malloc(psz);
    if(payload == 0){
      printf("serialtest: cannot allocate payload buffer\n");
      close(payload_fd);
      close(fd);
      exit(1);
    }
    int remaining = psz;
    char *p = payload;
    while(remaining > 0){
      int n = read(payload_fd, p, remaining);
      if(n < 0){
        printf("serialtest: read payload %s failed\n", argv[2]);
        free(payload);
        close(payload_fd);
        close(fd);
        exit(1);
      }
      if(n == 0){
        printf("serialtest: short read from payload %s\n", argv[2]);
        free(payload);
        close(payload_fd);
        close(fd);
        exit(1);
      }
      p += n;
      remaining -= n;
    }
    remaining = psz;
    p = payload;
    while(remaining > 0){
      int n = write(fd, p, remaining);
      if(n <= 0){
        printf("serialtest: write failed\n");
        free(payload);
        close(payload_fd);
        close(fd);
        exit(1);
      }
      p += n;
      remaining -= n;
    }
    free(payload);
    close(payload_fd);
  } else if(strcmp(argv[1], "loop") == 0){
    if(argc != 3){
      usage();
      close(fd);
      exit(1);
    }
    int want = atoi(argv[2]);
    if(want <= 0){
      printf("serialtest: bad read length\n");
      close(fd);
      exit(1);
    }
    char *buf = malloc(want);
    if(buf == 0){
      printf("serialtest: cannot allocate buffer\n");
      close(fd);
      exit(1);
    }
    int got = 0;
    while(got < want){
      int n = read(fd, buf + got, want - got);
      if(n < 0){
        printf("serialtest: read failed\n");
        free(buf);
        close(fd);
        exit(1);
      }
      // should be impossible as there is no EOF on a serial port.
      if(n == 0){
        printf("serialtest: short read\n");
        free(buf);
        close(fd);
        exit(1);
      }
      got += n;
    }
    int remaining = got;
    char *p = buf;
    while(remaining > 0){
      int n = write(fd, p, remaining);
      if(n <= 0){
        printf("serialtest: echo failed\n");
        free(buf);
        close(fd);
        exit(1);
      }
      p += n;
      remaining -= n;
    }
    free(buf);
  } else {
    usage();
    close(fd);
    exit(1);
  }

  close(fd);
  exit(0);
}
