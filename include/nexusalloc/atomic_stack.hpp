#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace nexusalloc {

class AtomicStack {
 public:
  AtomicStack() noexcept = default;

  // Non-copyable, non-movable
  AtomicStack(const AtomicStack&) = delete;
  AtomicStack& operator=(const AtomicStack&) = delete;
  AtomicStack(AtomicStack&&) = delete;
  AtomicStack& operator=(AtomicStack&&) = delete;

  void push(void* chunk) noexcept {
    if (chunk == nullptr) return;

    Node* new_node = static_cast<Node*>(chunk);
    TaggedPtr old_head = head_.load(std::memory_order_relaxed);
    TaggedPtr new_head;

    do {
      new_node->next = old_head.ptr;
      new_head.ptr = new_node;
      new_head.tag = old_head.tag + 1;  // Increment tag to prevent ABA
    } while (!head_.compare_exchange_weak(old_head, new_head, std::memory_order_release,
                                          std::memory_order_relaxed));
  }

  [[nodiscard]] void* pop() noexcept {
    TaggedPtr old_head = head_.load(std::memory_order_acquire);
    TaggedPtr new_head;

    do {
      if (old_head.ptr == nullptr) {
        return nullptr;
      }
      new_head.ptr = old_head.ptr->next;
      new_head.tag = old_head.tag + 1;  // Increment tag to prevent ABA
    } while (!head_.compare_exchange_weak(old_head, new_head, std::memory_order_acquire,
                                          std::memory_order_relaxed));

    return old_head.ptr;
  }

  [[nodiscard]] bool empty() const noexcept {
    return head_.load(std::memory_order_relaxed).ptr == nullptr;
  }

  [[nodiscard]] size_t approximate_size() const noexcept {
    size_t count = 0;
    Node* current = head_.load(std::memory_order_relaxed).ptr;
    while (current != nullptr && count < 1000000) {  // Safety limit
      ++count;
      current = current->next;
    }
    return count;
  }

 private:
  // node to store the next pointer in the first 8 bytes of the chunk
  struct Node {
    Node* next;
  };

  // Tagged pointer to solve the ABA problem
  // The tag is incremented on every CAS operation
  struct TaggedPtr {
    Node* ptr{nullptr};
    uint64_t tag{0};

    bool operator==(const TaggedPtr& other) const noexcept {
      return ptr == other.ptr && tag == other.tag;
    }
  };

  // Aligned to 16 bytes for 128-bit CAS atomic instruction
  alignas(16) std::atomic<TaggedPtr> head_{};
};

// Singleton for the global page stack
inline AtomicStack& global_page_stack() noexcept {
  static AtomicStack stack;
  return stack;
}

}  // namespace nexusalloc
