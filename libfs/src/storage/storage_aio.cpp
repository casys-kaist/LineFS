#include "storage/aio/async.h"
#include "storage/aio/buffer.h"

#include <fcntl.h>

struct AIOControl {
  std::map<uint8_t /* dev */, AIO::AIO> aios;

  AIO::AIO& get(uint8_t dev) {
    auto it = aios.find(dev);
    if (it != aios.end()) {
      return it->second;
    } else {
      std::cout << "Invalid device" << std::endl;
      exit(-1);
    }
  }
};

static AIOControl control;

extern "C" uint8_t mlfs_aio_init(uint8_t dev, char *dev_path) {
  int fd = open(dev_path, O_RDWR | O_DIRECT);
  size_t block_size = 4096;
  if (fd < 0) {
    perror("cannot open device");
    exit(-1);
  }
  int result = 1; // ioctl(fd, _IO(0x12, 104), &block_size); // BLKSSZGET
  if (result < 0) {
    std::cout << "Cannot get block size " << result << std::endl;
    exit(-1);
  }
  control.aios.emplace(std::piecewise_construct,
                       std::forward_as_tuple(dev),
                       std::forward_as_tuple(fd, block_size));
  return 0;
}

extern "C" int mlfs_aio_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size) {
  return control.get(dev).pread(buf, io_size, blockno << g_block_size_shift);
}

extern "C" int mlfs_aio_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size) {
  return control.get(dev).pwrite(buf, io_size, blockno << g_block_size_shift);
}

extern "C" int mlfs_aio_commit(uint8_t dev, addr_t _blockno, uint32_t _offset, uint32_t _io_size, int _flags) {
  return control.get(dev).commit();
}

extern "C" int mlfs_aio_erase(uint8_t dev, addr_t blockno, uint32_t io_size) {
  return control.get(dev).trim_block(blockno, io_size);
}

extern "C" int mlfs_aio_readahead(uint8_t dev, addr_t blockno, uint32_t io_size) {
  // TODO
  return control.get(dev).readahead(blockno, io_size);
}


extern "C" int mlfs_aio_wait_io(uint8_t dev, int read) {
  return control.get(dev).wait();
}

extern "C" int mlfs_aio_exit(uint8_t dev) {
  int fd = control.get(dev).file_descriptor();
  control.aios.erase(dev);
  close(fd);
  return 0;
}
