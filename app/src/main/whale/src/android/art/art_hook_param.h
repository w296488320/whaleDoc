#ifndef WHALE_ANDROID_ART_INTERCEPT_PARAM_H_
#define WHALE_ANDROID_ART_INTERCEPT_PARAM_H_

#include <jni.h>
#include "base/primitive_types.h"
#include "ffi_cxx.h"

namespace whale {
    namespace art {

        struct ArtHookParam final {
            //是否是静态 方法
            bool is_static_;
            // 当前方法的描述字符串
            const char *shorty_;
            //从java层传入过来的类被Hook信息的类 返回结果 参数
            // 那些 ，已经被设置成全局变量，不被回收
            jobject addition_info_;
            //二进制入口  oat文件的
            ptr_t origin_compiled_code_;
            //JNI的 入口
            ptr_t origin_jni_code_;
            //当前方法的 flag public 之类的
            u4 origin_access_flags;
            // 原始 codeItem的 偏移
            u4 origin_code_item_off;
            // 替换的  Method
            jobject origin_method_;
            //被Hook方法的 Method
            jobject hooked_method_;
            //所属class
            volatile ptr_t decl_class_;
            //classloader
            jobject class_Loader_;
            //被Hook 方法的 native
            jmethodID hooked_native_method_;
            //替换的方法
            jmethodID origin_native_method_;
            //最终处理的 地方
            FFIClosure *jni_closure_;
        };

    }  // namespace art
}  // namespace whale

#endif  // WHALE_ANDROID_ART_INTERCEPT_PARAM_H_
