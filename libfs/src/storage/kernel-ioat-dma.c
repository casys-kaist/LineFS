#include <fcntl.h>
#include <sys/ioctl.h>
#include "kernel-ioat-dma.h"

#ifdef IOAT_INTERRUPT_KERNEL_MODULE

/* Additional variables used in this API */
int kernel_ioat_fd = -1;
uint64_t *kernel_ioat_chans;
unsigned long *num_ioat_pending;

/* Defined variables by macro in storage_dax.c */
#define DMA_QUEUE_DEPTH 32
#define DMA_MAX_CHANNELS 8

int kernel_ioat_probe() {
  int ret;

  do {
    ret = ioctl(kernel_ioat_fd, IOCTL_IOAT_DMA_GET_DEVICE, &kernel_ioat_chans[n_ioat_chan]);
    printf("attaching to ioat device: %d\n", n_ioat_chan);
    n_ioat_chan++;
  } while (n_ioat_chan < DMA_MAX_CHANNELS && ret == 0);

  return ret;
}

int kernel_ioat_init() {
  kernel_ioat_fd = open("/dev/ioat-dma", O_RDWR);
  if (kernel_ioat_fd < 0) {
    panic("openinig ioat-dma kernel module failed");
  }

  n_ioat_chan = 0;
  kernel_ioat_chans = (uint64_t *) malloc(DMA_MAX_CHANNELS * sizeof(uint64_t));
  num_ioat_pending = (unsigned long *) malloc(DMA_MAX_CHANNELS * sizeof(unsigned long));
  if (!kernel_ioat_chans || !num_ioat_pending) {
	  panic("ioat channels malloc() failed");
  }

  for (int i=0; i<DMA_MAX_CHANNELS; i++) {
	  num_ioat_pending[i] = 0;
  }

  if (kernel_ioat_probe() != 0) {
    panic("unable to attach to ioat devices");
  }
}

int kernel_ioat_register(int dev) {
  // Currently use "devname" hardcoded value for specifying a device,
  // and there is not device registration API in ioat-dma kernel module.
  // This function does nothing but exists for signature consistency.
  return 0;
}

int kernel_ioat_read(uint8_t dev, addr_t buf, addr_t addr, uint32_t io_size) {
	int chan_id = dev & n_ioat_chan;
  // Current ioat-dma kernel module receives "offset", not a full address.
  // Assuming source and destination is the same device,
  // it calculates offset by subtracting the base address of the given device.
  struct ioctl_dma_args args = {
    .device_id = kernel_ioat_chans[chan_id],
    .device_name = devname,
    .src_offset = addr - ((addr_t) dax_addr[dev]),
    .dst_offset = buf - ((addr_t) dax_addr[dev]),
    .size = io_size
  };

  return ioctl(kernel_ioat_fd, IOCTL_IOAT_DMA_SUBMIT, &args);
}

int kernel_ioat_write(uint8_t dev, addr_t buf, addr_t addr, uint32_t io_size) {
  // Same offset calculation described in kernel_ioat_read.
  struct ioctl_dma_args args = {
    .device_id = kernel_ioat_chans[chan_id],
    .device_name = devname,
    .src_offset = buf - ((addr_t) dax_addr[dev]),
    .dst_offset = addr - ((addr_t) dax_addr[dev]),
    .size = io_size
  };
  int result = ioctl(kernel_ioat_fd, IOCTL_IOAT_DMA_SUBMIT, &args);
  num_ioat_pending[chan_id]++;

  if (num_ioat_pending[chan_id] == DMA_QUEUE_DEPTH) {
	  struct ioctl_dma_wait_args args = {
		  .device_id = kernel_ioat_chans[chan_id],
		  .result = 0
	  };
	  ioctl(kernel_ioat_fd, IOCTL_IOAT_DMA_WAIT_ALL, &args);

	  num_ioat_pending[chan_id] = 0;
  }

  return result;
}

#endif
