#pragma once

#include <list>
#include <iostream>
#include <deque>
#include <map>
#include <memory>
#include <cstring>

#include "global/types.h"
#include "assert.h"

namespace AIO {
  class BlockCache {
   private:

    struct CacheEntry {
      off_t offset;
      Buffer buffer;
    };

    std::deque<CacheEntry> entries_;
    const size_t max_buffers_;
    const size_t block_size_;

   public:
    BlockCache(size_t max_buffers, size_t block_size) : max_buffers_(max_buffers), block_size_(block_size) {}
    BlockCache(const BlockCache& o) = delete;
    void operator=(const BlockCache& o) = delete;

    void add(off_t offset, Buffer buffer) {
      // TODO: handle cases of overlapping buffers
      this->entries_.emplace_back(CacheEntry { offset, std::move(buffer) });
      if (this->entries_.size() >= this->max_buffers_) {
        this->entries_.pop_front();
      }
    }

    template <typename F>
    void visit(off_t offset, size_t size, F visitor) const {
      assert(offset % this->block_size_ == 0);
      assert(size % this->block_size_ == 0);
      for (const auto& entry : this->entries_) {
        if (entry.offset >= offset && entry.offset < (offset + size)) {
          visitor(entry.offset, entry.buffer);
        }
      }
    }

    size_t block_size() const { return block_size_; }

    void invalidate(off_t offset, size_t size) {
      this->entries_.clear();
    }
  };
}
