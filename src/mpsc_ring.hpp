// FROM: https://github.com/gubnik/mpsc-queue.git
// COPYRIGHT: GNU GPL v3 or higher, Nikolay Gubankov (nikgub)

#pragma once

#include "types.hpp"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <utility>

namespace ngg::mpsc {

/**
 * @brief Lossy MPSC wait-free queue based on a ring buffer for x86_64
 *
 * Generally at least 20% faster than @ref ngg::mpsc::stable_queue on smaller
 * sizes and up to 400% faster on large sizes (default allocator)
 * Very fast at the cost of extreme memory overhead.
 *
 * Uses token contract for wait-free element access, see @
 * ngg::mpsc::lossy_queue::slot
 *
 * @tparam T inner data type
 */
template <types::trasferable T>
class lossy_queue {
public:
  explicit lossy_queue(size_t capacity_pow2) {
    size_t cap = capacity_pow2;
    if (cap < 2 || (cap & (cap - 1)) != 0)
      throw std::invalid_argument("Capacity must be power of two >= 2");
    m_capacity = cap;
    m_mask = m_capacity - 1;
    m_slots = static_cast<slot*>(::operator new[](sizeof(slot) * m_capacity));
    for (size_t i = 0; i < m_capacity; ++i) {
      new (&m_slots[i]) slot();
      m_slots[i].token.store(i, std::memory_order_relaxed);
    }
    m_head.store(0, std::memory_order_relaxed);
    m_tail.store(0, std::memory_order_relaxed);
  }

  ~lossy_queue() {
    uint64_t t = m_tail.load(std::memory_order_relaxed);
    uint64_t h = m_head.load(std::memory_order_relaxed);
    while (t != h) {
      slot& s = m_slots[t & m_mask];
      T* ptr = reinterpret_cast<T*>(&s.storage);
      ptr->~T();
      ++t;
    }
    for (size_t i = 0; i < m_capacity; ++i)
      m_slots[i].~slot();
    ::operator delete[](m_slots);
  }

  lossy_queue(const lossy_queue&) = delete;
  lossy_queue& operator=(const lossy_queue&) = delete;
  lossy_queue(lossy_queue&&) = delete;
  lossy_queue& operator=(lossy_queue&&) = delete;

public:
  bool push(const T& value) {
    return emplace_impl(value);
  }

  bool push(T&& value) {
    return emplace_impl(std::move(value));
  }

  template <typename... Args>
  bool emplace(Args&&... args) {
    return emplace_impl(std::forward<Args>(args)...);
  }

  bool try_pull(T& out) {
    uint64_t tail = m_tail.load(std::memory_order_relaxed);
    slot& tail_slot = m_slots[tail & m_mask];
    uint64_t tail_seq = tail_slot.token.load(std::memory_order_acquire);
    if (tail_seq != tail + 1)
      return false;
    T* ptr = reinterpret_cast<T*>(&tail_slot.storage);
    out = std::move(*ptr);
    ptr->~T();
    tail_slot.token.store(tail + m_capacity, std::memory_order_release);
    m_tail.store(tail + 1, std::memory_order_relaxed);
    return true;
  }

  size_t capacity() const {
    return m_capacity;
  }

private:
  struct slot {
    /**
         * @brief token of the slot
         *
         * seq == idx       => producer can construct
         * seq == idx + 1   => consumer can claim
         * seq >= idx + cap => consumer claimed
         */
    std::atomic<uint64_t> token;
    alignas(T) std::byte storage[sizeof(T)];

    slot() : token(0) {}
    ~slot() = default;
  };

  template <typename... Args>
  bool emplace_impl(Args&&... args) {
    uint64_t head = m_head.fetch_add(1, std::memory_order_relaxed);
    slot& head_slot = m_slots[head & m_mask];
    uint64_t head_seq = head_slot.token.load(std::memory_order_acquire);
    if (head_seq != head) {
      // TODO: implement dropping mechanism, or don't. dropping introduces
      // a lot of overhead, not sure if it is needed. may be a candidate
      // for a separate class.
      return false;
    }
    T* ptr = reinterpret_cast<T*>(&head_slot.storage);
    new (ptr) T(std::forward<Args>(args)...);
    head_slot.token.store(head + 1, std::memory_order_release);
    return true;
  }

  slot* m_slots;
  size_t m_capacity;
  size_t m_mask;
  alignas(64) std::atomic<uint64_t> m_head;
  alignas(64) std::atomic<uint64_t> m_tail;
};

template <typename T>
using ring = lossy_queue<T>;

}  // namespace ngg::mpsc
