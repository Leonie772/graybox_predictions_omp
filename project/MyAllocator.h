#ifndef __MY_ALLOCATOR_H
#define __MY_ALLOCATOR_H

#include <memory>
#include <iostream>
#include <limits>
#include <dlfcn.h>
#include <unistd.h>

template<typename T>
class MyAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = MyAllocator<U>;
    };

    MyAllocator() noexcept {}
    template<typename U>
    MyAllocator(const MyAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        return static_cast<pointer>(((void *(*)(size_t)) dlsym(RTLD_NEXT, "malloc"))(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type n) noexcept {
       ((void (*)(void *)) dlsym(RTLD_NEXT, "free"))(p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new (p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) noexcept {
        p->~U();
    }
};

// Comparison operators are required for allocators
template<typename T, typename U>
bool operator==(const MyAllocator<T>&, const MyAllocator<U>&) noexcept {
    return true;
}

template<typename T, typename U>
bool operator!=(const MyAllocator<T>&, const MyAllocator<U>&) noexcept {
    return false;
}

#endif //__MY_ALLOCATOR_H
