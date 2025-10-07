// Prints the `text` to stdout, appends '\n' and flushes the stream in a thread-safe manner.
// Should not be used by the final logger implementation. Useful for debugging.
#include "mpsc_ring.hpp"
#include <cstdio>

void print(std::string_view text);

// Helper function, that reverses a singly linked list.
template <class T>
constexpr T* reverse(T* head) noexcept {
  // nikgub: no-op for the implementation
  /*T* list = nullptr;
  while (head) {
    const auto next = head->next;
    head->next = list;
    list = head;
    head = next;
  }*/
  return head;
}

// =================================================================================================
// Multithreading
// =================================================================================================
// Task: Implement `logger` as a multiple producers, singele consumer queue as efficient as you can.

/**
 * @brief Logger implemented via a bounded MPSC queue
 */
class logger {
public:
  logger(size_t capacity_pow2 = static_cast<size_t>(16 * 1'024 * 1'024)) : queue_(capacity_pow2) {}

  // Queues the message. Called from multiple threads.
  void post(std::string text) {
    while (!queue_.emplace(std::move(text)))
      ;
  }

  // Processes messages. Called from a single thread.
  void run(std::stop_token stop) {
    while (!stop.stop_requested()) {
      std::string elem;
      while (queue_.try_pull(elem))
        std::fputs(elem.data(), stdout);
    }
  }

private:
  ngg::mpsc::ring<std::string> queue_;
};

#define logger logger
