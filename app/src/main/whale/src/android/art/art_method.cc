#include "android/art/art_method.h"
#include "well_known_classes.h"

namespace whale {
    namespace art {

        jobject ArtMethod::Clone(JNIEnv *env, u4 access_flags) {
            // 这个clone是 ArtMethod 里面的 clone的一个封装
            int32_t api_level = GetAndroidApiLevel();
            //生成的 jmethod 相当于返回的 ArtMethod
            jmethodID jni_clone_method = nullptr;
            if (api_level < ANDROID_M) {
                //如果小于6.0 不包括6 直接调用 里面的 CloneArtObject
                // 这个函数 不同版本 里面的 都不一样
                // art::mirror::Object::Clone(art:: Thread*)
                // art::mirror::Object::Clone(art:: Thread*,art::mirror::Class*)
                // art::mirror::Object::Clone(art:: Thread*,unsigned long)
                // art::mirror::Object::Clone(art:: Thread*,unsigned int)
                jni_clone_method =
                        reinterpret_cast<jmethodID>(ArtRuntime::Get()->CloneArtObject(jni_method_));
            } else {
                //大于 6.0
                //先分配 内存
                jni_clone_method = reinterpret_cast<jmethodID>(malloc(offset_->method_size_));
                //这块 判断 是否存在 CopyFrom  没有的话 直接调用 memcpy
                //用 Copyfrom是 最好的办法
                if (symbols_->ArtMethod_CopyFrom) {
                    symbols_->ArtMethod_CopyFrom(jni_clone_method, jni_method_, sizeof(ptr_t));
                } else {
                    memcpy(jni_clone_method, jni_method_, offset_->method_size_);
                }
            }

            //jmethodID转换成 ArtMethod
            ArtMethod clone_method = ArtMethod(jni_clone_method);
            //调整 flag
            bool is_direct_method = (access_flags & kAccDirectFlags) != 0;
            bool is_native_method = (access_flags & kAccNative) != 0;
            if (!is_direct_method) {
                //将 非公开的方法 设置成 provate
                access_flags &= ~(kAccPublic | kAccProtected);
                access_flags |= kAccPrivate;
            }
            //去除 Synchronized关键字
            access_flags &= ~kAccSynchronized;
            //阻止JIT编译
            if (api_level < ANDROID_O_MR1) {
                access_flags |= kAccCompileDontBother_N;
            } else {
                access_flags |= kAccCompileDontBother_O_MR1;
                access_flags |= kAccPreviouslyWarm_O_MR1;
            }
            if (!is_native_method) {
                access_flags |= kAccSkipAccessChecks;
            }
            if (api_level >= ANDROID_N) {
                //7.0  将 方法的 HotnessCount设置成 0 防止 JIT编译
                //如果调用 CopyFrom 的话 其实已经做完了的
                clone_method.SetHotnessCount(0);
                if (!is_native_method) {
                    //拿到 原函数的Native的 入口地址
                    ptr_t profiling_info = GetEntryPointFromJni();
                    if (profiling_info != nullptr) {
                        offset_t end = sizeof(u4) * 4;
                        //为了兼容 老版本 开始搜索
                        for (offset_t offset = 0; offset != end; offset += sizeof(u4)) {
                            if (MemberOf<ptr_t>(profiling_info, offset) == jni_method_) {
                                AssignOffset<ptr_t>(profiling_info, offset, jni_clone_method);
                            }
                        }
                    }
                }
            }
            if (!is_native_method && symbols_->art_quick_to_interpreter_bridge) {
                clone_method.SetEntryPointFromQuickCompiledCode(
                        symbols_->art_quick_to_interpreter_bridge);
            }

            clone_method.SetAccessFlags(access_flags);

            bool is_constructor = (access_flags & kAccConstructor) != 0;
            bool is_static = (access_flags & kAccStatic) != 0;
            if (is_constructor) {
                clone_method.RemoveAccessFlags(kAccConstructor);
            }
            jobject java_method = env->ToReflectedMethod(WellKnownClasses::java_lang_Object,
                                                         jni_clone_method,
                                                         static_cast<jboolean>(is_static));
            env->CallVoidMethod(java_method,
                                WellKnownClasses::java_lang_reflect_AccessibleObject_setAccessible,
                                true);
            if (is_constructor) {
                clone_method.AddAccessFlags(kAccConstructor);
            }
            return java_method;
        }


    }  // namespace art
}  // namespace whale
