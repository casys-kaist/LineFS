#pragma once

#include <memory>
#include <vector>
#include <map>
#include <iostream>
#include <functional>
#include <cstring>

#include "buffer.h"
#include "cache.h"

#include <sys/ioctl.h>
#include <libaio.h>
#include <unistd.h>

#include "global/types.h"
#include "global/util.h"
#include "assert.h"


#ifdef MLFS_DEBUG
#define CHECK_ALIGNMENT(ptr, alignment) \
  if (reinterpret_cast<uintptr_t>((ptr)) % (alignment) != 0) {        \
    std::cout << "Pointer " << (ptr) << " is not aligned to block size " << (alignment) << std::endl; \
    abort(); \
  }
#else
#define CHECK_ALIGNMENT(ptr, alignment)
#endif

namespace AIO {
  class AIO {
   private:

    static constexpr size_t max_queue_size = 64;

    struct AIOCallback {
      std::function<void(Buffer)> callback_;

      template <typename T>
      AIOCallback(T cb) : callback_(std::move(cb)) {}

      void operator()(Buffer buffer) {
        this->callback_(std::move(buffer));
      }
    };

    struct SubmittedIO {
      std::unique_ptr<struct iocb> cb;
      uint64_t submission_timestamp;
      uint64_t completed_timestamp;
      CallbackBuffer<AIOCallback> buffer;
      SubmittedIO(std::unique_ptr<struct iocb> _cb, CallbackBuffer<AIOCallback> _buffer) : cb(std::move(_cb)), submission_timestamp(asm_rdtscp()), completed_timestamp(0), buffer(std::move(_buffer)) {}
    };

    const int fd_;
    const size_t block_size_;
    io_context_t ctx_;
    std::map<uint64_t /* submission timestamp */, std::unique_ptr<SubmittedIO>> submitted_ios_;
    std::vector<struct io_event> events_;
    BlockCache cache_;
    off_t file_offset_;

    size_t cached_tsc_ = 0;
    size_t cached_n_ = 0;
    size_t cached_read_total_ = 0;
    size_t uncached_tsc_ = 0;
    size_t uncached_n_ = 0;
    size_t uncached_read_total_ = 0;
    size_t readahead_total_ = 0;
    size_t read_tsc_ = 0;
    size_t read_n_ = 0;
    size_t read_total_ = 0;

   private:

    int submit_io(CallbackBuffer<AIOCallback> buffer, off_t offset, short io_code) {
      if (this->submitted_ios_.size() >= max_queue_size) {
        return -1;
      }

      std::unique_ptr<struct iocb> cb(new struct iocb());
      cb->aio_lio_opcode = io_code;
      cb->aio_fildes = this->fd_;
      cb->u.c.buf = buffer.get();
      CHECK_ALIGNMENT(cb->u.c.buf, this->block_size_);
      cb->u.c.offset = offset;
      cb->u.c.nbytes = buffer.size();

      struct iocb* borrowed_cb = cb.get();

      std::unique_ptr<SubmittedIO> sio(new SubmittedIO(std::move(cb), std::move(buffer)));
      borrowed_cb->data = sio.get(); // establish link
      this->submitted_ios_.emplace(std::piecewise_construct, std::forward_as_tuple(sio->submission_timestamp), std::forward_as_tuple(std::move(sio)));
      this->events_.resize(this->submitted_ios_.size());
      int result = io_submit(this->ctx_, 1, &borrowed_cb);
      static_cast<SubmittedIO *>(borrowed_cb->data)->completed_timestamp = asm_rdtscp();
      if (result >= 0) {
        assert(result == 1);
      } else {
        std::cout << "result " << result << std::endl;
        // panic
        throw std::string("failed to submit IO");
      }
      return result;
    }

    int maybe_cached_read(void* const buffer, const size_t length, const off_t offset) {
      assert(offset % this->block_size_ == 0);
      assert(length % this->block_size_ == 0);
      CHECK_ALIGNMENT(buffer, this->block_size_);
      uint64_t s = asm_rdtscp();
      this->poll();
      off_t io_offset = offset;
      int result = 0;
      this->cache_.visit(offset, length, [&](const off_t current_offset, const Buffer& cache_buffer) {
                                           if (result < 0) return;
                                           if (io_offset != current_offset) {
                                             assert(io_offset < current_offset);
                                             uint64_t f = asm_rdtscp();
                                             int pread_result = ::pread(this->fd_, static_cast<uint8_t*>(buffer) + (io_offset - offset), current_offset - io_offset, io_offset);
                                             if (pread_result < 0) { result = pread_result; return; };
                                             this->uncached_read_total_ += pread_result;
                                             this->uncached_n_ += 1;
                                             this->uncached_tsc_ += asm_rdtscp() - f;
                                             result += pread_result;
                                             assert(io_offset + pread_result == current_offset);
                                           }
                                           uint64_t f = asm_rdtscp();
                                           void* target_buf = static_cast<uint8_t*>(buffer) + (current_offset - offset);
                                           size_t n_copy = std::min(cache_buffer.size(), length - (current_offset - offset));
                                           std::memcpy(target_buf, cache_buffer.get(), n_copy);
                                           this->cached_read_total_ += n_copy;
                                           this->cached_n_ += 1;
                                           this->cached_tsc_ += asm_rdtscp() - f;
                                           result += n_copy;
                                           io_offset = current_offset + n_copy;
                                   });
      if (io_offset != (offset + length) && result >= 0) {
        assert(io_offset < (offset + length));
        uint64_t f = asm_rdtscp();
        size_t final_pread = ::pread(this->fd_, static_cast<uint8_t*>(buffer) + (io_offset - offset), (offset + length) - io_offset, io_offset);
        this->uncached_read_total_ += final_pread;
        this->uncached_n_ += 1;
        this->uncached_tsc_ += asm_rdtscp() - f;
        result += final_pread;
      }
      read_tsc_ += asm_rdtscp() - s;
      read_n_ += 1;
      if (result >= 0) {
        read_total_ += result;
      }
      return result;
    }

   public:
    explicit AIO(int fd, size_t block_size) : fd_(fd), block_size_(block_size), ctx_(nullptr), cache_(2, block_size) {
      int result = io_setup(max_queue_size, &this->ctx_);
      if (result < 0) {
        std::cout << "result " << result << std::endl;
        // panic
        throw std::string("failed to io_setup");
      }
    }

    ~AIO() {
#ifdef MLFS_DEBUG
      std::cout << "cached reads " << cached_read_total_ << " uncached reads " << uncached_read_total_  << " readahead " << readahead_total_ << std::endl;
      std::cout << "cached n " << cached_n_ << " total " << cached_read_total_ << ":" << (double) cached_tsc_ / cached_read_total_ << " tsc " << cached_tsc_ << ":" << (double) cached_tsc_ / cached_n_ << std::endl;
      std::cout << "uncached n " << uncached_n_ << " total " << uncached_read_total_ << ":" << (double) uncached_tsc_ / uncached_read_total_ << " tsc " << uncached_tsc_ << ":" << (double) uncached_tsc_ / uncached_n_ << std::endl;
      std::cout << "read n " << read_n_ << " total " << read_total_ << ":" << (double) read_tsc_ / read_total_ << " tsc " << read_tsc_ << ":" << (double) read_tsc_ / read_n_ << std::endl;
#endif
      io_destroy(this->ctx_);
    }


    template <typename CallbackT>
    int submit_read(Buffer buffer, off_t offset, CallbackT callback) {
      auto io_size = buffer.size();
      int result = this->submit_io(std::move(CallbackBuffer<AIOCallback>(std::move(buffer), AIOCallback(std::move(callback)))), offset, IO_CMD_PREAD);
      return result;
    }

    template <typename CallbackT>
    int submit_write(Buffer buffer, off_t offset, CallbackT callback) {
      auto io_size = buffer.size();
      int result = this->submit_io(std::move(CallbackBuffer<AIOCallback>(std::move(buffer), AIOCallback(std::move(callback)))), offset, IO_CMD_PWRITE);
      return result;
    }

    int readahead(addr_t block, size_t length) {
      assert(length % this->block_size_ == 0);
      off_t offset = block * this->block_size_;
    retry:
      int result = this->submit_read(std::move(Buffer(length, this->block_size_)), offset, [this, offset](Buffer b) {
                                                                                this->cache_.add(offset, std::move(b));
                                                                        });
      if (result < 0) {
        this->wait();
        goto retry;
      }
      this->readahead_total_ += length;
      return result;
    }

    int pread(void* buffer, size_t length, off_t offset) {
      if (reinterpret_cast<uintptr_t>(buffer) % this->block_size_ != 0) {
        Buffer aligned_buffer(length, this->block_size_);
        int result = maybe_cached_read(aligned_buffer.get(), length, offset);
        std::memcpy(buffer, aligned_buffer.get(), length);
        return result;
      } else {
        return maybe_cached_read(buffer, length, offset);
      }
    }

    int read(void* buffer, size_t length) {
      int result = this->pread(buffer, length, this->file_offset_);
      if (result >= 0) {
        this->file_offset_ += result;
      }
      return result;
    }

    int pwrite(void* buffer, size_t length, off_t offset) {
      this->cache_.invalidate(offset, length);
      if (reinterpret_cast<uintptr_t>(buffer) % this->block_size_ != 0) {
        Buffer aligned_buffer(length, this->block_size_);
        std::memcpy(aligned_buffer.get(), buffer, length);
        int result = ::pwrite(this->fd_, aligned_buffer.get(), length, offset);
        return result;
      } else {
        return ::pwrite(this->fd_, buffer, length, offset);
      }
    }

    int write(void* buffer, size_t length) {
      int result = this->pwrite(buffer, length, this->file_offset_);
      if (result >= 0) {
        this->file_offset_ += result;
      }
      return result;
    }

    off_t seek(off_t offset) {
      this->file_offset_ = offset;
      return offset;
    }

    int commit() {
      return fdatasync(this->fd_);
    }

    int trim_block(addr_t block, size_t size) {
      this->cache_.invalidate(block * this->block_size_, size);
      off_t ioarg[2];
      ioarg[0] = block;
      ioarg[1] = size;
      return ioctl(this->fd_, _IOW('a', 104, off_t[2]), ioarg); // IOCATADELETE
    }

    int pread(Buffer buffer, off_t offset) {
      int result = this->pread(buffer.get(), buffer.size(), offset);
      return result;
    }

    int pwrite(Buffer buffer, off_t offset) {
      int result = this->pwrite(buffer.get(), buffer.size(), offset);
      return result;
    }

    size_t poll(uint64_t* total_elapsed_cycles = nullptr, struct timespec* timeout = nullptr, size_t min_nr = 0) {
      if (this->submitted_ios_.size() == 0) {
        return 0;
      }
      this->events_.clear();
      this->events_.resize(this->submitted_ios_.size());
      int num_events = io_getevents(this->ctx_, std::min(min_nr, this->submitted_ios_.size()), this->submitted_ios_.size() /* max_nr */, this->events_.data(), timeout);
      if (num_events < 0) {
        std::cout << "poll failed " << num_events << std::endl;
        // panic
        throw std::string("poll failed");
      }
      for (size_t i = 0; i < num_events; i++) {
        SubmittedIO* sio = static_cast<SubmittedIO*>(this->events_[i].data);
        //sio->completed_timestamp = asm_rdtscp();
        if (total_elapsed_cycles != nullptr) *total_elapsed_cycles += sio->completed_timestamp - sio->submission_timestamp;
        uint64_t key = sio->submission_timestamp;
        sio->buffer.perform_callback();
        size_t erased = this->submitted_ios_.erase(key);
        if (erased != 1) {
          std::cout << "erased should be 1" << std::endl;
          throw std::string("erased not 1");
        }
      }
      return num_events;
    }

    size_t wait(uint64_t* total_elapsed_cycles = nullptr) {
      size_t r = 0;
      while (this->submitted_ios_.size() > 0) {
        r += this->poll(total_elapsed_cycles, nullptr, this->submitted_ios_.size());
#if (defined(__i386__) || defined(__x86_64__))
        asm volatile("pause\n": : :"memory");
#elif (defined(__aarch64__))
        asm volatile("yield\n": : :"memory");
#else
#error "Not supported architecture."
#endif
      }
      return r;
    }

    int file_descriptor() const {
      return this->fd_;
    }

    size_t block_size() const {
      return this->block_size_;
    }
  };
}
