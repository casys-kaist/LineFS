#include "membench.h"
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <memory>

#include "time_stat.h"
#include "thread.h"
#include "storage/aio/async.h"

//pcie-ssd
//const char test_dir_prefix[] = "./ssd";
//sata-ssd
//const char test_dir_prefix[] = "/data";
//const char test_dir_prefix[] = "./";
//harddisk
const char test_dir_prefix[] = "/backup/mlfs_ssd";

#define block_size_bytes 4096
#define block_size_shift 12

typedef enum {SEQ_WRITE, SEQ_READ, SEQ_WRITE_READ, RAND_WRITE, RAND_READ, READ_LATENCY, NONE} test_t;

typedef enum {FS, NVM, DRAM} test_mode_t;

typedef unsigned long addr_t;

/****************************************************
 *
 *    DRAM io functions
 *
 *****************************************************/
static uint8_t *dram_base_addr;
int dram_init(uint8_t dev, ssize_t map_size)
{
  dram_base_addr = (uint8_t *)mmap(NULL, map_size, PROT_READ | PROT_WRITE,
      MAP_ANONYMOUS| MAP_PRIVATE| MAP_POPULATE, -1, 0);

  if (dram_base_addr == MAP_FAILED) {
    perror("cannot map dram area\n");
    exit(-1);
  }

  return 0;
}

int dram_read(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
  memmove(buf, dram_base_addr + (blockno * block_size_bytes), io_size);

  return 0;
}

int dram_write(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
  memmove(dram_base_addr + (blockno * block_size_bytes), buf, io_size);

  return 0;
}

int dram_erase(uint8_t dev, uint32_t blockno, addr_t io_size)
{
  return 0;
}

void dram_exit(uint8_t dev, ssize_t map_size)
{
  munmap(dram_base_addr, map_size);

  return;
}

/****************************************************
 *
 *    NVM emulation io functions
 *
 *****************************************************/

// Set CPU frequency correctly
#define _CPUFREQ 2600LLU /* MHz */

#define NS2CYCLE(__ns) (((__ns) * _CPUFREQ) / 1000)
#define CYCLE2NS(__cycles) (((__cycles) * 1000) / _CPUFREQ)

#define ENABLE_PERF_MODEL
#define ENABLE_BANDWIDTH_MODEL

// performance parameters
/* SCM read extra latency than DRAM */
uint32_t SCM_EXTRA_READ_LATENCY_NS = 150;

/* SCM write bandwidth */
uint32_t SCM_BANDWIDTH_MB = 8000;
/* DRAM system peak bandwidth */
uint32_t DRAM_BANDWIDTH_MB = 63000;

static uint32_t bandwidth_consumption = 0;
static uint64_t monitor_start = 0, monitor_end = 0, now = 0;

static inline void emulate_latency_ns(int ns)
{
    uint64_t cycles, start, stop;

    start = asm_rdtsc();
    cycles = NS2CYCLE(ns);
    //printf("cycles %lu\n", cycles);

    do {
        /* RDTSC doesn't necessarily wait for previous instructions to complete
         * so a serializing instruction is usually used to ensure previous
         * instructions have completed. However, in our case this is a desirable
         * property since we want to overlap the latency we emulate with the
         * actual latency of the emulated instruction.
         */
        stop = asm_rdtsc();
    } while (stop - start < cycles);
}

static void perfmodel_add_delay(int read, size_t size)
{
#ifdef ENABLE_PERF_MODEL
    uint32_t extra_latency;
  uint32_t do_bandwidth_delay;

  now = asm_rdtscp();

  if (now >= monitor_end) {
    monitor_start = now;
    monitor_end = NS2CYCLE(1000);
    bandwidth_consumption = 0;
  }

  bandwidth_consumption += size;

  if (bandwidth_consumption >= (SCM_BANDWIDTH_MB << 20))
    do_bandwidth_delay = 1;
  else
    do_bandwidth_delay = 0;
#endif

#ifdef ENABLE_PERF_MODEL
    if (read) {
        extra_latency = SCM_EXTRA_READ_LATENCY_NS;
    } else
        extra_latency = 0;

  // bandwidth delay for both read and write.
  if (do_bandwidth_delay) {
    // Due to the writeback cache, write does not have latency
    // but it has bandwidth limit.
    // The following is emulated delay when bandwidth is full
    extra_latency += (int)size *
      (1 - (float)(((float) SCM_BANDWIDTH_MB)/1000) /
       (((float)DRAM_BANDWIDTH_MB)/1000)) / (((float)SCM_BANDWIDTH_MB)/1000);
    // Bandwidth is enough, so no write delay.
  }

    //printf("latency %u ns\n", extra_latency);
    emulate_latency_ns(extra_latency);
#endif

    return;
}

int dax_init(uint8_t dev, ssize_t map_size)
{
  dram_base_addr = (uint8_t *)mmap(NULL, map_size, PROT_READ | PROT_WRITE,
      MAP_ANONYMOUS| MAP_PRIVATE| MAP_POPULATE, -1, 0);

  if (dram_base_addr == MAP_FAILED) {
    perror("cannot map dram area\n");
    exit(-1);
  }

  return 0;
}

int dax_read(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
  memmove(buf, dram_base_addr + (blockno * block_size_bytes), io_size);
  perfmodel_add_delay(1, io_size);

  return 0;
}

int dax_write(uint8_t dev, uint64_t *buf, addr_t blockno, uint32_t io_size)
{
  memmove(dram_base_addr + (blockno * block_size_bytes), buf, io_size);
  perfmodel_add_delay(0, io_size);

  return 0;
}

void dax_exit(uint8_t dev, ssize_t map_size)
{
  munmap(dram_base_addr, map_size);

  return;
}

/****************************************************
 *
 *    mem_bench class. each function (init, read, write) contains a switch
 *    to call the correct io device
 *
 *****************************************************/
class mem_bench : public CThread
{
  public:
    mem_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
        test_t _test_type, test_mode_t _test_mode);

    int id, fsfd;
    std::unique_ptr<AIO::AIO> aio;
    unsigned long file_size_bytes;
    unsigned int io_size;
    test_t test_type;
    test_mode_t test_mode;
    int do_fsync;
    uint64_t *buf;
    struct time_stats stats;
    ssize_t offset;

    void prepare(void);

    int io_init(void);
    ssize_t io_read(int fd, void *buf, size_t count);
    ssize_t io_write(int fd, void *buf, size_t count);
    off_t io_lseek(int fd, off_t offset, int whence);

    void do_read(void);
    void do_write(void);
    void do_read_latency(void);
    void do_exit(void);

    // Thread entry point.
    void Run(void);

    // util methods
    static unsigned long str_to_size(char* str);
    static test_t get_test_type(char *);
    static test_mode_t get_test_mode(char *);
    static void hexdump(void *mem, unsigned int len);
    static void show_usage(const char *prog);
};

mem_bench::mem_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
    test_t _test_type, test_mode_t _test_mode)
  : id(_id), file_size_bytes(_file_size_bytes), io_size(_io_size),
  test_type(_test_type), test_mode(_test_mode)
{
  offset = 0;
}

int mem_bench::io_init(void)
{
  //per storage initialization
  switch (test_mode) {
    case FS:
      break;
    case NVM:
      dax_init(0, file_size_bytes);
      break;
    case DRAM:
      dram_init(0, file_size_bytes);
      break;
  }
  return 0;
}

ssize_t mem_bench::io_read(int fd, void *buf, size_t count)
{
  int ret;
  switch (test_mode) {
    case FS:
      ret = aio->read(buf, count);
      if (ret < 0)
        err(1, "read err\n");
      break;
    case NVM:
      dax_read(0, (uint64_t *)buf, offset >> block_size_shift , count);
      offset += count;
      break;
    case DRAM:
      dram_read(0, (uint64_t *)buf, offset >> block_size_shift , count);
      offset += count;

      break;
  }

  return count;
}

ssize_t mem_bench::io_write(int fd, void *buf, size_t count)
{
  int ret;
  switch (test_mode) {
    case FS:
      ret = aio->write(buf, count);
      break;
    case NVM:
      dax_write(0, (uint64_t *)buf, offset >> block_size_shift, count);
      offset += count;
      break;
    case DRAM:
      dram_write(0, (uint64_t *)buf, offset >> block_size_shift, count);
      offset += count;
      break;
  }

  return count;
}

void mem_bench::do_exit(void)
{
  switch (test_mode) {
    case FS:
      break;
    case NVM:
      break;
    case DRAM:
      break;
  }
}

off_t mem_bench::io_lseek(int fd, off_t _offset, int whence)
{
  switch (test_mode) {
    case FS:
      aio->seek(_offset);
      break;
    case NVM:
      offset = _offset;
      break;
    case DRAM:
      offset = _offset;
      break;
  }
  return 0;
}

void mem_bench::prepare(void)
{
  int ret;

  if (test_mode == FS) {
    char file_path[256];
    unsigned int len = sprintf(file_path,"%s", test_dir_prefix);

    ret = mkdir(test_dir_prefix, 0777);

    if (ret < 0 && errno != EEXIST) {
      perror("mkdir\n");
      exit(-1);
    }

    fsfd = open(file_path, O_RDWR | O_CREAT | O_DIRECT,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fsfd < 0) {
      err(1, "open");
    }
    aio = std::move(std::unique_ptr<AIO::AIO>(new AIO::AIO(fsfd, 4096)));
  }

  do_fsync = 1;
  io_init();

  //buf = new uint64_t[file_size_bytes/8];
  //buf = new uint64_t[io_size];
  buf = (uint64_t*) aligned_alloc(1024 * 1024, io_size);
  srand(time(NULL));
}

void mem_bench::do_write(void)
{
  int fd = 0;
  unsigned long random_range;

  random_range = file_size_bytes / io_size;

  for (unsigned long i = 0; i < file_size_bytes/8; i++)
    buf[i] = '0' + (i % 10);

  /**
   *  Run each test 3 so we have variance
   */
#define N_TESTS 1

  time_stats_init(&stats, N_TESTS);
  for (int test = 0 ; test < N_TESTS ; test++) {
    /* start timer */
    time_stats_start(&stats);

    /**
     *  Do (size/rw-int) write operations
     */
    int bytes_written;
    for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
      if(test_type == RAND_WRITE) {
        // TODO: fix this with c11 rand function.
        unsigned int rand_io_offset = rand() % random_range;
        io_lseek(fd, rand_io_offset*io_size, SEEK_SET);
      }

      if (i + io_size > file_size_bytes)
        io_size = file_size_bytes - i;
      else
        io_size = io_size;
    ISSUE_WRITE:
      bytes_written = io_write(fd, (uint8_t *)buf+i, io_size);

      if (bytes_written != io_size) {
        printf("write request %u received len %d\n",
            io_size, bytes_written);
        errx(1, "write");
      }
    }

    /**
     * EXT4 sync/flush
     */
    if (test_mode == FS && do_fsync) {
      printf("do_sync\n");
      fsync(fsfd);
    }

    time_stats_stop(&stats);

    /**
     * clean up after testing and quit
     */
    if(test_mode == FS) {
      close(fsfd);
    }

    //end of tests loop
  }

  time_stats_print(&stats, (char *)"---------------");

  printf("Throughput: %3.3f MB/s\n",(float)(file_size_bytes)
      / (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));

  return ;
}

void mem_bench::do_read(void)
{
  int fd;
  unsigned long i;
  int ret;

  printf("File size %lu bytes\n", (unsigned long)file_size_bytes);

  memset(buf, 1, file_size_bytes);
  time_stats_init(&stats, 1);

  time_stats_start(&stats);

  for (i = 0; i < file_size_bytes ; i += io_size) {
    if (i + io_size > file_size_bytes)
      io_size = file_size_bytes - i;
    else
      io_size = io_size;

ISSUE_READ:
    ret = io_read(fd, (uint8_t *)buf + i, io_size);
    /*
       if (ret != io_size) {
       printf("read size mismatch: return %d, request %lu\n",
       ret, io_size);
       }
       */
  }

  time_stats_stop(&stats);

  /*
     for (unsigned long i = 0; i < file_size_bytes; i++) {
     int bytes_read = read(fd, buf+i, io_size + 100);

     if (bytes_read != io_size) {
     printf("read too far: length %d\n", bytes_read);
     }
     }
     */

#if 0
  if (test_mode == FS) {
    // Read data integrity check.
    for (unsigned long i = 0; i < file_size_bytes/8; i++) {
      if (buf[i] != '0' + (i % 10)) {
        hexdump(buf + i, 256);
        printf("read data mismatch at %lu\n", i);
        printf("expected %c read %c\n", (int)('0' + (i % 10)), buf[i]);
        exit(-1);
      }
    }
    printf("Read data matches\n");
  }
#endif

  time_stats_print(&stats, (char *)"---------------");

  printf("%f\n", (float) time_stats_get_avg(&stats));

  printf("Throughput: %3.3f MB\n",(float)((file_size_bytes) >> 20)
      / (float) time_stats_get_avg(&stats));

  if (test_mode == FS)
    close(fsfd);

  return ;
}


static uint64_t bigrand(void) {
  return ((uint64_t)rand()) << 32 | rand();
}

void mem_bench::do_read_latency(void) {
  int fd;
  unsigned long i = 0;
  int ret;

  printf("File size %lu(<%lu) bytes\n", (unsigned long)io_size, (unsigned long)file_size_bytes);

  memset(buf, 1, io_size);
  time_stats_init(&stats, 1);

  time_stats_start(&stats);

  uint64_t total_tsc = 0;
  uint64_t total = 0;
  uint64_t total_io = 0;

  //aio->readahead(0, file_size_bytes);
  while (total < 100000) {
    uint64_t offset = 0; //((bigrand() % (file_size_bytes - io_size)) >> 12) << 12;
    //uint64_t ftsc = asm_rdtscp();

    io_lseek(fd, offset, SEEK_SET);

    for (i = 0; i < file_size_bytes ; i += io_size) {

      if (i + io_size > file_size_bytes)
        io_size = file_size_bytes - i;
      else
        io_size = io_size;

      uint64_t ftsc = asm_rdtscp();

    ISSUE_READ:
      ret = io_read(fd, (uint8_t *)buf, io_size);
      /*
        if (ret != io_size) {
        printf("read size mismatch: return %d, request %lu\n",
        ret, io_size);
        }
      */
      uint64_t stsc = asm_rdtscp();
      total_tsc += stsc - ftsc;
      total += 1;
      total_io += io_size;
    }
  }

  time_stats_stop(&stats);

  /*
    for (unsigned long i = 0; i < file_size_bytes; i++) {
    int bytes_read = read(fd, buf+i, io_size + 100);

    if (bytes_read != io_size) {
    printf("read too far: length %d\n", bytes_read);
    }
    }
  */
  time_stats_print(&stats, (char *)"---------------");

  printf("%f\n", (float) time_stats_get_avg(&stats));
  printf("Latency: %f cycles (total: %llu num: %llu)\n", (double) total_tsc / total, total_tsc, total);

  printf("Throughput: %3.3f MB\n",(float)((total_io) >> 20)
         / (float) time_stats_get_avg(&stats));

  if (test_mode == FS)
    close(fsfd);

  return ;
}

void mem_bench::Run(void)
{
  cout << "thread " << id << " start - ";

  if (test_type == SEQ_READ || test_type == RAND_READ)
    this->do_read();
  else if (test_type == SEQ_WRITE || test_type == RAND_WRITE || test_type == SEQ_WRITE_READ)
    this->do_write();
  else if (test_type == READ_LATENCY)
    this->do_read_latency();

  if (test_type == SEQ_WRITE_READ)
    this->do_read();

  this->do_exit();

  //delete buf;

  return;
}

/*
   void mem_bench::Join(void)
   {
   cout << CThread::done << endl;
   CThread::Join();
   }
*/

unsigned long mem_bench::str_to_size(char* str)
{
  /* magnitude is last character of size */
  char size_magnitude = str[strlen(str)-1];
  /* erase magnitude char */
  str[strlen(str)-1] = 0;
  unsigned long file_size_bytes = strtoull(str, NULL, 0);
  switch(size_magnitude) {
    case 'g':
    case 'G':
      file_size_bytes *= 1024;
    case 'm':
    case 'M':
      file_size_bytes *= 1024;
    case '\0':
    case 'k':
    case 'K':
      file_size_bytes *= 1024;
      break;
    case 'p':
    case 'P':
      file_size_bytes *= 4;
      break;
    default:
      std::cout << "incorrect size format " << str << endl;
      break;
  }
  return file_size_bytes;
}

test_t mem_bench::get_test_type(char *test_type)
{
  /**
   * Check the mode to bench: read or write and type
   */
  if (!strcmp(test_type, "sr")){
    return SEQ_READ;
  }
  else if (!strcmp(test_type, "sw")) {
    return SEQ_WRITE;
  }
  else if (!strcmp(test_type, "wr")) {
    return SEQ_WRITE_READ;
  }
  else if (!strcmp(test_type, "rw")) {
    return RAND_WRITE;
  }
  else if (!strcmp(test_type, "rr")) {
    return RAND_READ;
  } else if (!strcmp(test_type, "rl")) {
    return READ_LATENCY;
  }
  else {
    show_usage("iobench");
    cerr << "unsupported test type" << test_type << endl;
    exit(-1);
  }
}

test_mode_t mem_bench::get_test_mode(char *test_mode)
{
  test_mode_t storage_type = FS;
  /**
   *  Test mode, ext4
   */
  if (!strcmp(test_mode, "fs")) {
    storage_type = FS;
  }
  else if (!strcmp(test_mode, "nvm")) {
    storage_type = NVM;
  }
  else if (!strcmp(test_mode, "dram")) {
    storage_type = DRAM;
  }
  else {
    show_usage("iobench");
    cerr << "unsupported test mode " << test_mode << endl;
    exit(-1);
  }

  return storage_type;
}

#define HEXDUMP_COLS 8
void mem_bench::hexdump(void *mem, unsigned int len)
{
  unsigned int i, j;

  for(i = 0; i < len + ((len % HEXDUMP_COLS) ?
        (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
    /* print offset */
    if(i % HEXDUMP_COLS == 0) {
      printf("0x%06x: ", i);
    }

    /* print hex data */
    if(i < len) {
      printf("%02x ", 0xFF & ((char*)mem)[i]);
    } else {/* end of block, just aligning for ASCII dump */
      printf("	");
    }

    /* print ASCII dump */
    if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
      for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
        if(j >= len) { /* end of block, not really printing */
          printf(" ");
        } else if(isprint(((char*)mem)[j])) { /* printable char */
          printf("%c",(0xFF & ((char*)mem)[j]));
        } else {/* other char */
          printf(".");
        }
      }
      printf("\n");
    }
  }
}

void mem_bench::show_usage(const char *prog)
{
  std::cerr << "usage: " << prog
    << " <wr/sr/sw/rr/rw> <fs/nvm/dram>"
    << " <size: X{G,M,K,P}, eg: 100M> <IO size, e.g.: 4K> <# of thread>"
    << endl;
}

int main(int argc, char *argv[])
{
  int n_threads, i;
  std::vector<mem_bench *> io_workers;
  unsigned long file_size_bytes;
  unsigned int io_size = 0;

  int fd, ret;
  int do_fflush = 0, do_fsync = 0;
  unsigned long random_range;
  struct timeval t_start,t_end,t_elap;
  float sec;
  struct stat f_stat;

  ssize_t (*write_fn)(int, const void*, size_t);
  off_t (*seek_fn)(int, off_t, int);

  if (argc != 6) {
    mem_bench::show_usage(argv[0]);
    exit(-1);
  }

  n_threads = std::stoi(argv[5]);

  file_size_bytes = mem_bench::str_to_size(argv[3]);
  io_size = mem_bench::str_to_size(argv[4]);

  std::cout << "Total file size: " << file_size_bytes << "B" << endl
    << "io size: " << io_size << "KB" << endl
    << "# of thread: " << n_threads << endl;

  for (i = 0; i < n_threads; i++) {
    io_workers.push_back(new mem_bench(i,
          file_size_bytes,
          io_size,
          mem_bench::get_test_type(argv[1]),
          mem_bench::get_test_mode(argv[2])));
  }

  for (auto it : io_workers)
    it->prepare();

  cout << "start experiment!" << endl;
  for (auto it : io_workers)
    it->Start();

  for (auto it : io_workers)
    it->Join();

  fflush(stdout);
  fflush(stderr);

  return 0;
}
