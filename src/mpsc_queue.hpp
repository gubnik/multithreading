// FROM: https://github.com/gubnik/mpsc-queue.git
// COPYRIGHT: GNU GPL v3 or higher, Nikolay Gubankov (nikgub)
//
#pragma once

#include "types.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace ngg::mpsc {

/**
 * @brief multiple producers/single consumer queue for x86_64
 *
 * Implements Michael-Scott queue, is intrusive and unbound.
 * Uses dummy sentinel node for initial head and tail.
 *
 * @tparam T type of inner data, must be default-constructible
 * @tparam Allocator allocator type, used for inner nodes
 */
template <typename T, types::minimal_allocator_type<T> Allocator = std::allocator<T>>
class stable_queue {
  // nikgub: semantics, a must-have
protected:
  struct node;

  using value_type = T;
  using pointer = node*;
  using atomic_node = std::atomic<pointer>;
  using t_allocator_traits = typename std::allocator_traits<Allocator>;
  using node_allocator = t_allocator_traits::template rebind_alloc<node>;
  using node_allocator_traits = typename std::allocator_traits<node_allocator>;

public:
  /**
     * @brief Constructs the queue.
     *
     * Inits the head and tail nodes with a dummy node.
     */
  stable_queue() : m_node_alloc() {
    pointer dummy = node_allocator_traits::allocate(m_node_alloc, 1);
    node_allocator_traits::construct(m_node_alloc, dummy, nullptr);
    // nikgub: relaxed memory since we do not contest anything yet
    m_head.store(dummy, std::memory_order_relaxed);
    m_tail.store(dummy, std::memory_order_relaxed);
  }

  stable_queue(const stable_queue&) = delete;
  stable_queue operator=(const stable_queue&) = delete;

  stable_queue(stable_queue&&) = delete;
  stable_queue operator=(stable_queue&&) = delete;

  /**
     * @brief Destroys the queue.
     *
     * Is tested and proven to not leave any leaks
     */
  ~stable_queue() {
    clear();  // leaves a single tail == head node
    node_allocator_traits::destroy(m_node_alloc, m_head.load());
    node_allocator_traits::deallocate(m_node_alloc, m_head.load(), 1);
  }

public:
  /**
     * @brief Copies a value to the queue.
     *
     * @param value value being copied
     */
  void push(const T& value) {
    emplace_impl(value);
  }

  /**
     * @brief Pushes an rvalue to the queue.
     *
     * Implemented as forwarding.
     *
     * @param value value being forwarded
     */
  void push(T&& value) {
    emplace_impl(std::move(value));
  }

  /**
     * @brief constructs a new T object from provided args.
     *
     * @param args constructor args
     */
  template <typename... Args>
  void emplace(Args&&... args) {
    emplace_impl(std::move(args)...);
  }

  /**
     * @brief Pops the first element from the queue.
     *
     * @returns value if any, nullopt otherwise
     */
  std::optional<T> pull() {
    pointer tail_ptr = m_tail.load(std::memory_order_relaxed);
    // nikgub: acquire the element
    pointer next = tail_ptr->next.load(std::memory_order_acquire);
    [[unlikely]] if (next == nullptr)  // nikgub: nullopt if none
      return std::nullopt;
    std::optional<T> result = std::move(next->data);
    m_tail.store(next, std::memory_order_release);
    node_allocator_traits::destroy(m_node_alloc, tail_ptr);
    node_allocator_traits::deallocate(m_node_alloc, tail_ptr, 1);
    return result;
  }

  /**
     * @brief Clears all the elements in the queue.
     */
  void clear() {
    pointer tail_ptr = m_tail.load(std::memory_order_relaxed);
    while (true) {
      pointer next = tail_ptr->next.load(std::memory_order_acquire);
      if (next == nullptr)
        break;
      m_tail.store(next, std::memory_order_release);
      node_allocator_traits::destroy(m_node_alloc, tail_ptr);
      node_allocator_traits::deallocate(m_node_alloc, tail_ptr, 1);
      tail_ptr = next;
    }
  }

protected:
  /**
     * @brief Inner node struct of mpsc_queue.
     *
     * Owns the inner data.
     */
  struct node {
    /**
         * @brief Argument constructor.
         *
         * Constructs inner data from an arg, sets the next pointer.
         */
    template <typename... Args>
    explicit node(node* next, Args&&... args) : next(next), data(std::forward<Args>(args)...) {}

    ~node() = default;

    atomic_node next = nullptr;
    std::optional<T> data = std::nullopt;
  };

private:
  node_allocator m_node_alloc;  // nikgub: alloc instance to support stateful allocators
  // nikgub: align for 64 bytes for x86_64.
  //         do this to prevent false sharing.
  //         No plans for 32bit support any time soon.
  //         TODO: maybe fix this?
  alignas(64) atomic_node m_head;  // nikgub: newest node
  alignas(64) atomic_node m_tail;  // nikgub: oldest node

private:
  /**
     * @brief Implementation of push.
     *
     * Uses forwarding to address all value types.
     * @note possible contentions in prev_head segment but does not break until
     * a miracle happens.
     *
     * @param value forwarding reference
     */
  template <typename... Args>
  void emplace_impl(Args&&... args) {
    pointer new_node = node_allocator_traits::allocate(m_node_alloc, 1);
    node_allocator_traits::construct(m_node_alloc, new_node, nullptr, std::forward<Args>(args)...);
    // nikgub: contested but fine
    // TODO: find a test where it fails
    pointer prev_head = m_head.exchange(new_node, std::memory_order_acq_rel);
    prev_head->next.store(new_node, std::memory_order_release);
  }
};

}  // namespace ngg::mpsc
