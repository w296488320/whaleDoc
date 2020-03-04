#ifndef WHALE_BASE_CXX_HELPER_H_
#define WHALE_BASE_CXX_HELPER_H_

#include <cstdlib>
#include "base/primitive_types.h"

template<typename U, typename T>
U ForceCast(T *x) {
    return (U) (uintptr_t) x;
}

template<typename U, typename T>
U ForceCast(T &x) {
    return *(U *) &x;
}

template<typename T>
struct Identity {
    using type = T;
};

template<typename R>
static inline R OffsetOf(uintptr_t ptr, size_t offset) {
    return reinterpret_cast<R>(ptr + offset);
}

template<typename R>
static inline R OffsetOf(intptr_t ptr, size_t offset) {
    return reinterpret_cast<R>(ptr + offset);
}

template<typename R>
static inline R OffsetOf(ptr_t ptr, size_t offset) {
    return (R) (reinterpret_cast<intptr_t>(ptr) + offset);
}


// 判断当前偏移是否属于指定指针
template<typename T>
static inline T MemberOf(ptr_t ptr, size_t offset) {
    return *OffsetOf<T *>(ptr, offset);
}

static inline size_t DistanceOf(ptr_t a, ptr_t b) {
    //static_cast 不同种类之间的强转 abs返回的是 int 强转成size_t
    return static_cast<size_t>(
            //reinterpret_cast 常用于指针类型的强转
            abs(reinterpret_cast<intptr_t>(b) - reinterpret_cast<intptr_t>(a))
    );
}

template<typename T>
static inline void AssignOffset(ptr_t ptr, size_t offset, T member) {
    *OffsetOf<T *>(ptr, offset) = member;
}

#endif  // WHALE_BASE_CXX_HELPER_H_
