#include "net/buffer.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace mini_redis {

Buffer::Buffer(std::size_t initial_size)
    : storage_(std::max<std::size_t>(initial_size, 1)) {}

std::size_t Buffer::readable_bytes() const noexcept {
  return write_index_ - read_index_;
}

std::size_t Buffer::writable_bytes() const noexcept {
  return storage_.size() - write_index_;
}

bool Buffer::empty() const noexcept {
  return readable_bytes() == 0;
}

const char* Buffer::peek() const noexcept {
  return storage_.data() + read_index_;
}

void Buffer::retrieve(std::size_t length) noexcept {
  if (length < readable_bytes()) {
    read_index_ += length;
    return;
  }

  retrieve_all();
}

void Buffer::retrieve_all() noexcept {
  read_index_ = 0;
  write_index_ = 0;
}

std::string Buffer::retrieve_as_string(std::size_t length) {
  const std::size_t count = std::min(length, readable_bytes());
  std::string result(peek(), count);
  retrieve(count);
  return result;
}

std::string Buffer::retrieve_all_as_string() {
  return retrieve_as_string(readable_bytes());
}

void Buffer::append(const char* data, std::size_t length) {
  if (length == 0) {
    return;
  }

  ensure_writable_bytes(length);
  std::memcpy(storage_.data() + write_index_, data, length);
  write_index_ += length;
}

void Buffer::append(std::string_view data) {
  append(data.data(), data.size());
}

void Buffer::ensure_writable_bytes(std::size_t length) {
  if (writable_bytes() >= length) {
    return;
  }

  const std::size_t readable = readable_bytes();
  if (read_index_ + writable_bytes() >= length) {
    std::memmove(storage_.data(), peek(), readable);
    read_index_ = 0;
    write_index_ = readable;
    return;
  }

  if (length > std::numeric_limits<std::size_t>::max() - write_index_) {
    throw std::length_error("Buffer size overflow");
  }
  storage_.resize(write_index_ + length);
}

}  // namespace mini_redis
