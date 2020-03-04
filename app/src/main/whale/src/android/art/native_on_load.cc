#include "android/jni_helper.h"
#include "android/art/art_runtime.h"
#include "base/logging.h"

#define CLASS_NAME "com/lody/whale/WhaleRuntime"

#ifndef WHALE_ANDROID_AUTO_LOAD
#define JNI_OnLoad Whale_OnLoad
#endif


extern "C" OPEN_API void WhaleRuntime_reserved0(JNI_START) {}

extern "C" OPEN_API void WhaleRuntime_reserved1(JNI_START) {}


static jlong WhaleRuntime_hookMethodNative(JNI_START, jclass decl_class, jobject method_obj,
                                           jobject addition_info) {
    //拿到 Art虚拟机
    auto runtime = whale::art::ArtRuntime::Get();

    return runtime->HookMethod(env, decl_class, method_obj, addition_info);
}


static jobject
WhaleRuntime_invokeOriginalMethodNative(JNI_START, jlong slot, jobject this_object,
                                        jobjectArray args) {
    auto runtime = whale::art::ArtRuntime::Get();
    return runtime->InvokeOriginalMethod(slot, this_object, args);
}


//拿到 某个Method实体类的 slot 在Method的 Index
static jlong
WhaleRuntime_getMethodSlot(JNI_START, jclass decl_class, jobject method_obj) {
    auto runtime = whale::art::ArtRuntime::Get();
    return runtime->GetMethodSlot(env, decl_class, method_obj);
}

static void
WhaleRuntime_setObjectClassNative(JNI_START, jobject obj, jclass parent_class) {
    auto runtime = whale::art::ArtRuntime::Get();
    return runtime->SetObjectClass(env, obj, parent_class);
}

static jobject
WhaleRuntime_cloneToSubclassNative(JNI_START, jobject obj, jclass sub_class) {
    auto runtime = whale::art::ArtRuntime::Get();
    return runtime->CloneToSubclass(env, obj, sub_class);
}

static void
WhaleRuntime_removeFinalFlagNative(JNI_START, jclass java_class) {
    auto runtime = whale::art::ArtRuntime::Get();
    runtime->RemoveFinalFlag(env, java_class);
}

void WhaleRuntime_enforceDisableHiddenAPIPolicy(JNI_START) {
    auto runtime = whale::art::ArtRuntime::Get();
    runtime->EnforceDisableHiddenAPIPolicy();
}


static JNINativeMethod gMethods[] = {
        NATIVE_METHOD(WhaleRuntime, hookMethodNative,
                      "(Ljava/lang/Class;Ljava/lang/reflect/Member;Ljava/lang/Object;)J"),
        NATIVE_METHOD(WhaleRuntime, invokeOriginalMethodNative,
                      "(JLjava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;"),
        NATIVE_METHOD(WhaleRuntime, getMethodSlot, "(Ljava/lang/reflect/Member;)J"),
        NATIVE_METHOD(WhaleRuntime, setObjectClassNative, "(Ljava/lang/Object;Ljava/lang/Class;)V"),
        NATIVE_METHOD(WhaleRuntime, cloneToSubclassNative,
                      "(Ljava/lang/Object;Ljava/lang/Class;)Ljava/lang/Object;"),
        NATIVE_METHOD(WhaleRuntime, removeFinalFlagNative,
                      "(Ljava/lang/Class;)V"),
        NATIVE_METHOD(WhaleRuntime, enforceDisableHiddenAPIPolicy, "()V"),
        NATIVE_METHOD(WhaleRuntime, reserved0, "()V"),
        NATIVE_METHOD(WhaleRuntime, reserved1, "()V")
};



//  JNI_OnLoad 函数入口
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOG(ERROR) << " 开始执行 native_on_load JNI_OnLoad  " << CLASS_NAME;

    JNIEnv *env = nullptr;
    //从vm虚拟机里面拿到 JNIEnv 判断是否初始化成功
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    //获取java层class  com/lody/whale/WhaleRuntime
    jclass cl = env->FindClass(CLASS_NAME);
    if (cl == nullptr) {
        LOG(ERROR) << "FindClass failed for " << CLASS_NAME;
        return JNI_ERR;
    }
    //将com/lody/whale/WhaleRuntime  Native 方法都注册了
    if (env->RegisterNatives(cl, gMethods, NELEM(gMethods)) < 0) {
        LOG(ERROR) << "RegisterNatives failed for " << CLASS_NAME;
        return JNI_ERR;
    }
    //得到  ArtRuntime 实体
    auto runtime = whale::art::ArtRuntime::Get();

    //这块对虚拟机进行初始化
    if (!runtime->OnLoad(vm, env, cl)) {
        LOG(ERROR) << "Runtime setup failed";
        return JNI_ERR;
    }
    return JNI_VERSION_1_4;
}
