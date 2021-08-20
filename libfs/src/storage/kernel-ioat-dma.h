#ifndef _KERNEL_IOAT_DMA_H_
#define _KERNEL_IOAT_DMA_H_

#ifdef IOAT_INTERRUPT_KERNEL_MODULE

#include <stdint.h>
#include "global/global.h"

// Current ioat-dma kernel module implementation requires device name to perform DMA.
// Now hard-coded for simple implementation.
#define devname "/dev/dax0.0"

struct ioctl_dma_args {
  uint64_t device_id;
  char device_name[32];
  uint64_t src_offset;
  uint64_t dst_offset;
  uint64_t size;
} __attribute__ ((packed));

struct ioctl_dma_wait_args {
  uint64_t device_id;
  uint64_t result;
} __attribute__ ((packed));


#define IOCTL_MAGIC 0xad
#define IOCTL_IOAT_DMA_SUBMIT _IOW(IOCTL_MAGIC, 0, struct ioctl_dma_args)
#define IOCTL_IOAT_DMA_GET_DEVICE_NUM _IOR(IOCTL_MAGIC, 0, uint32_t)
#define IOCTL_IOAT_DMA_GET_DEVICE _IOR(IOCTL_MAGIC, 1, uint64_t)
#define IOCTL_IOAT_DMA_WAIT_ALL _IOWR(IOCTL_MAGIC, 0, struct ioctl_dma_wait_args)

/* defined in kernel-ioat-dma.c */
extern int kernel_ioat_fd;
extern uint64_t *kernel_ioat_chans;

/* defined in storage_dax.c and used by this API */
extern int n_ioat_chan;
extern int chan_alloc;
extern __thread int chan_id;
extern uint8_t *dax_addr[];

/* defined in storage_dax.c but not used in this API */
// int g_socket_id = 1;
// int ioat_attached = 0;
// int last_ioat_socket = -1;
// int copy_done[DMA_MAX_CHANNELS][DMA_QUEUE_DEPTH];
// int *ioat_pending;
// struct spdk_ioat_chan **ioat_chans;
// pthread_mutex_t ioat_mutex[DMA_MAX_CHANNELS];

/* New API declarations */
int kernel_ioat_probe(); /* equivalent to spdk_ioat_probe() */
int kernel_ioat_init();  /* equivalent to ioat_init() */
int kernel_ioat_register(int dev); /* equivalent to ioat_register() */
int kernel_ioat_read(uint8_t dev, addr_t buf, addr_t addr, uint32_t io_size); /* equivalent to ioat_read() */
int kernel_ioat_write(uint8_t dev, addr_t buf, addr_t addr, uint32_t io_size); /* equivalent to ioat_write() */

#endif

#endif
