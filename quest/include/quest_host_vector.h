#pragma once

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

/* OpenXR containers outlive game heaps and also allocate before JKR initializes. */
template <class T> struct QuestHostAllocator {
    using value_type = T;
    using is_always_equal = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;

    QuestHostAllocator() noexcept = default;
    template <class U> QuestHostAllocator(const QuestHostAllocator<U>&) noexcept {}
    template <class U> struct rebind { using other = QuestHostAllocator<U>; };

    T* allocate(std::size_t count) {
        static_assert(alignof(T) <= alignof(std::max_align_t), "Host vector requires malloc-compatible alignment");
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc();
        void* memory = std::malloc(count ? count * sizeof(T) : 1);
        if (!memory) throw std::bad_alloc();
        return static_cast<T*>(memory);
    }

    void deallocate(T* memory, std::size_t) noexcept { std::free(memory); }
};

template <class T, class U>
bool operator==(const QuestHostAllocator<T>&, const QuestHostAllocator<U>&) noexcept { return true; }
template <class T, class U>
bool operator!=(const QuestHostAllocator<T>&, const QuestHostAllocator<U>&) noexcept { return false; }

template <class T> using QuestHostVector = std::vector<T, QuestHostAllocator<T>>;
