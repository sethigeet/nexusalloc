#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace nexusalloc::internal {

// Fixed-size bitmap for tracking block occupancy
// Uses 64-bit words for efficient operations
template <size_t NumBits>
class Bitmap {
 public:
  static constexpr size_t kBitsPerWord = 64;
  static constexpr size_t kNumWords = (NumBits + kBitsPerWord - 1) / kBitsPerWord;

  constexpr Bitmap() noexcept = default;

  constexpr void set(size_t index) noexcept {
    auto [word_idx, bit_idx] = decompose(index);
    words_[word_idx] |= (1ULL << bit_idx);
  }

  constexpr void clear(size_t index) noexcept {
    auto [word_idx, bit_idx] = decompose(index);
    words_[word_idx] &= ~(1ULL << bit_idx);
  }

  [[nodiscard]] constexpr bool test(size_t index) const noexcept {
    auto [word_idx, bit_idx] = decompose(index);
    return (words_[word_idx] & (1ULL << bit_idx)) != 0;
  }

  [[nodiscard]] constexpr size_t count() const noexcept {
    size_t total = 0;
    for (const auto& word : words_) {
      total += static_cast<size_t>(std::popcount(word));
    }
    return total;
  }

  [[nodiscard]] constexpr bool none() const noexcept {
    for (const auto& word : words_) {
      if (word != 0) return false;
    }
    return true;
  }

  [[nodiscard]] constexpr bool all() const noexcept {
    // Check all fully used words
    for (size_t i = 0; i < kNumWords - 1; ++i) {
      if (words_[i] != std::numeric_limits<uint64_t>::max()) return false;
    }
    // Check last word (may have unused bits)
    constexpr size_t last_word_bits = NumBits % kBitsPerWord;
    if constexpr (last_word_bits == 0) {
      return words_[kNumWords - 1] == std::numeric_limits<uint64_t>::max();
    } else {
      constexpr uint64_t last_word_mask = (1ULL << last_word_bits) - 1;
      return words_[kNumWords - 1] == last_word_mask;
    }
  }

  [[nodiscard]] constexpr size_t find_first_clear() const noexcept {
    for (size_t i = 0; i < kNumWords; ++i) {
      uint64_t inverted = ~words_[i];
      if (inverted != 0) {
        size_t bit_in_word = static_cast<size_t>(std::countr_zero(inverted));
        size_t global_idx = i * kBitsPerWord + bit_in_word;
        return (global_idx < NumBits) ? global_idx : NumBits;
      }
    }
    return NumBits;
  }

  constexpr void reset() noexcept { words_.fill(0); }

  [[nodiscard]] static constexpr size_t size() noexcept { return NumBits; }

 private:
  [[nodiscard]] static constexpr std::pair<size_t, size_t> decompose(size_t index) noexcept {
    return {index / kBitsPerWord, index % kBitsPerWord};
  }

  std::array<uint64_t, kNumWords> words_{};
};

}  // namespace nexusalloc::internal
