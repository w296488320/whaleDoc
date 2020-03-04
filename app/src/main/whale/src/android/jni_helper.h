#ifndef WHALE_ANDROID_ART_JNI_HELPER_H_
#define WHALE_ANDROID_ART_JNI_HELPER_H_

#include <jni.h>
#include <base/macros.h>

#define NATIVE_METHOD(className, functionName, signature) \
    { #functionName, signature, reinterpret_cast<void*>(className ## _ ## functionName) }

#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define JNI_START JNIEnv *env, jclass cl

static inline void JNIExceptionClear(JNIEnv *env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}
// inline 对应频繁使用的函数 可以加上 在编译期间直接进行替换
// 防止 栈开销过大的 问题 一般用在小型的函数里面
static inline bool JNIExceptionCheck(JNIEnv *env) {
    //这块是检测是否出现异常 因为 在JNI里面操作
    if (env->ExceptionCheck()) {
        //发现异常
        jthrowable e = env->ExceptionOccurred();
        //抛出去
        env->Throw(e);
        env->DeleteLocalRef(e);
        return true;
    }
    return false;
}

static inline void JNIExceptionClearAndDescribe(JNIEnv *env) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

template<typename T>
class ScopedLocalRef {
 public:
    ScopedLocalRef(JNIEnv *env, T localRef) : mEnv(env), mLocalRef(localRef) {
    }

    ~ScopedLocalRef() {
        reset();
    }

    void reset(T ptr = nullptr) {
        if (ptr != mLocalRef) {
            if (mLocalRef != nullptr) {
                mEnv->DeleteLocalRef(mLocalRef);
            }
            mLocalRef = ptr;
        }
    }

    T release() {
        T localRef = mLocalRef;
        mLocalRef = nullptr;
        return localRef;
    }

    T get() const {
        return mLocalRef;
    }

 private:
    JNIEnv *const mEnv;
    T mLocalRef;

    DISALLOW_COPY_AND_ASSIGN(ScopedLocalRef);
};

#endif  // WHALE_ANDROID_ART_JNI_HELPER_H_
