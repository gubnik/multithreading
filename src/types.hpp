// FROM: https://github.com/gubnik/mpsc-queue.git
// COPYRIGHT: GNU GPL v3 or higher, Nikolay Gubankov (nikgub)

#pragma once

#include <concepts>
#include <type_traits>

namespace ngg::types {

template <typename Tt>
concept trasferable = std::is_move_constructible_v<Tt> || std::is_copy_constructible_v<Tt>;

/**
 * @brief Concept for a default-constructible type
 *
 * @tparam Tt type to validate
 */
template <typename Allocator, typename U>
concept minimal_allocator_type = requires(Allocator alloc, U* mem, std::size_t n) {
  { alloc.allocate(n) } -> std::convertible_to<U*>;
  { alloc.deallocate(mem, n) } -> std::same_as<void>;
};

}  // namespace ngg::types
