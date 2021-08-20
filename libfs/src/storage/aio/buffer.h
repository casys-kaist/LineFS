#pragma once

#include <vector>
#include <iostream>
#include "global/types.h"

namespace AIO {

  class Buffer {
    void* ptr_;
    size_t len_;
    size_t alignment_;

   public:
    Buffer(void* ptr, size_t len, size_t alignment) : ptr_(ptr), len_(len), alignment_(alignment) {
      if (reinterpret_cast<uintptr_t>((ptr)) % alignment != 0) {
        std::cout << "Pointer " << (ptr) << " is not aligned to block size " << (alignment) << std::endl;
        abort();
      }
    }
    Buffer(size_t len, size_t alignment) : ptr_(aligned_alloc(alignment, len)), len_(len), alignment_(alignment) {}

    Buffer(Buffer&& o) : ptr_(nullptr), len_(0) {
      std::swap(this->ptr_, o.ptr_);
      std::swap(this->len_, o.len_);
      std::swap(this->alignment_, o.alignment_);
    }
    Buffer(const Buffer& o) = delete;
    void operator=(const Buffer& o) = delete;
    void operator=(Buffer&& o) {
      if (this->ptr_ != nullptr) {
        free(this->ptr_);
      }
      std::swap(this->ptr_, o.ptr_);
      std::swap(this->len_, o.len_);
      std::swap(this->alignment_, o.alignment_);
    }
    ~Buffer() {
      if (this->ptr_ != nullptr) {
        free(this->ptr_);
      }
    }
    void* get() const { return this->ptr_; }
    size_t size() const { return this->len_; }
    size_t alignment() const { return this->alignment_; }
  };

  template <typename Callback>
  class CallbackBuffer {
    Buffer buffer_;
    Callback callback_;

   public:
    CallbackBuffer(Buffer&& buffer, Callback&& callback) : buffer_(std::move(buffer)), callback_(std::move(callback)) {}
    CallbackBuffer(CallbackBuffer&& o) : buffer_(std::move(o.buffer_)), callback_(std::move(o.callback_)) {
    }
    void operator=(CallbackBuffer&& o) {
      std::swap(o.callback_, this->callback_);
      std::swap(o.buffer_, this->buffer_);
    }
    void operator=(const CallbackBuffer& o) = delete;
    CallbackBuffer(const CallbackBuffer& o) = delete;

    void* get() const { return this->buffer_.get(); }
    size_t size() const { return this->buffer_.size(); }
    size_t alignment() const { return this->buffer_.alignment(); }
    void perform_callback() {
      callback_(std::move(this->buffer_));
    }
  };
}
