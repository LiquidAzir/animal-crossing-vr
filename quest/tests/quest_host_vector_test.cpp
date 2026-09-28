#include "quest_host_vector.h"
#include <cstdint>
#include <cstdio>
#include <utility>
#if defined(__ANDROID__)
#include <EGL/egl.h>
#else
using EGLenum = unsigned int; // Header-only OpenXR GLES types on the Windows host.
#endif
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

static volatile unsigned game_new_calls, game_delete_calls;
static volatile bool game_heap_available = true;
static volatile const void* escaped;
static unsigned checks, failures;

// Model the game's overridden new, which has no available heap at XR startup.
void* operator new(std::size_t bytes) {
    ++game_new_calls;
    if (!game_heap_available) throw std::bad_alloc();
    void* p = std::malloc(bytes ? bytes : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { ++game_delete_calls; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

#define CHECK(expression) do { ++checks; if (!(expression)) { \
    ++failures; std::printf("line %u: %s\n", __LINE__, #expression); } } while (0)

template<class T> static void exercise_type() {
    QuestHostVector<T> items;
    CHECK(items.empty());
    for (unsigned n = 1; n <= 65; ++n) {
        items.resize(n);
        CHECK(items.size() == n);
        CHECK(reinterpret_cast<std::uintptr_t>(items.data()) % alignof(T) == 0);
    }
    const auto capacity = items.capacity();
    items.reserve(capacity + 127);
    CHECK(items.capacity() >= capacity + 127);
    QuestHostVector<T> copy(items);
    CHECK(copy.size() == items.size());
    T* storage = copy.data();
    QuestHostVector<T> moved(std::move(copy));
    CHECK(moved.data() == storage);
    copy = items;
    CHECK(copy.size() == 65);
    copy = std::move(moved);
    CHECK(copy.data() == storage);
    copy.swap(items);
    CHECK(items.data() == storage);
    copy.clear();
    CHECK(copy.empty());
    copy.shrink_to_fit();
    CHECK(copy.capacity() == 0);
}

struct Lifetime {
    static unsigned live;
    int value = 719;
    Lifetime() { ++live; }
    Lifetime(const Lifetime& other) : value(other.value) { ++live; }
    Lifetime(Lifetime&& other) noexcept : value(other.value) { ++live; }
    Lifetime& operator=(const Lifetime&) = default;
    Lifetime& operator=(Lifetime&&) = default;
    ~Lifetime() { --live; }
};
unsigned Lifetime::live;

__attribute__((noinline)) static void ordinary_vector_control() {
    std::vector<int> ordinary(193);
    escaped = ordinary.data();
}

int main() {
    game_heap_available = false;
    bool failed_without_game_heap = false;
    try { ordinary_vector_control(); }
    catch (const std::bad_alloc&) { failed_without_game_heap = true; }
    CHECK(failed_without_game_heap && game_new_calls == 1);
    game_new_calls = game_delete_calls = 0;

    // These are the element types used by runtime discovery, swapchains, and input.
    exercise_type<XrExtensionProperties>();
    exercise_type<std::int64_t>();
    exercise_type<XrSwapchainImageOpenGLESKHR>();
    exercise_type<std::uint32_t>();
    exercise_type<XrActionSuggestedBinding>();
    exercise_type<std::max_align_t>();
    exercise_type<Lifetime>();
    CHECK(Lifetime::live == 0);
    {
        QuestHostVector<Lifetime> persistent(3);
        CHECK(Lifetime::live == 3);
        game_heap_available = true; // Game begins; existing XR storage remains host-owned.
        persistent.resize(1000);
        CHECK(persistent[0].value == 719 && Lifetime::live == 1000);
        game_heap_available = false; // Game heap is torn down before XR container destruction.
    }
    CHECK(Lifetime::live == 0);
    QuestHostAllocator<std::uint64_t> allocator;
    auto* zero = allocator.allocate(0);
    CHECK(zero != nullptr);
    allocator.deallocate(zero, 0);
    bool overflow_rejected = false;
    try { escaped = allocator.allocate(std::numeric_limits<std::size_t>::max() / sizeof(std::uint64_t) + 1); }
    catch (const std::bad_alloc&) { overflow_rejected = true; }
    CHECK(overflow_rejected);
    CHECK(allocator == QuestHostAllocator<std::uint32_t>{});
    CHECK(!(allocator != QuestHostAllocator<std::uint32_t>{}));
    CHECK(game_new_calls == 0 && game_delete_calls == 0);
    game_heap_available = true;
    std::printf("checks=%u failures=%u host_vector_game_new=%u host_vector_game_delete=%u\n",
                checks, failures, game_new_calls, game_delete_calls);
    return failures ? 1 : 0;
}
