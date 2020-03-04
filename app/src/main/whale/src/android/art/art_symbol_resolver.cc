#include "whale.h"
#include "android/art/art_symbol_resolver.h"
#include "android/android_build.h"
#include "android/art/art_runtime.h"

#define SYMBOL static constexpr const char *

namespace whale {
namespace art {

// art::ArtMethod::CopyFrom(art::ArtMethod*, art::PointerSize)
SYMBOL kArtMethod_CopyFrom_O = "_ZN3art9ArtMethod8CopyFromEPS0_NS_11PointerSizeE";
#if defined(__LP64__)
// art::ArtMethod::CopyFrom(art::ArtMethod*, unsigned long)
SYMBOL kArtMethod_CopyFrom_N = "_ZN3art9ArtMethod8CopyFromEPS0_m";
// art::ArtMethod::CopyFrom(art::ArtMethod const*, unsigned long)
SYMBOL kArtMethod_CopyFrom_M = "_ZN3art9ArtMethod8CopyFromEPKS0_m";
#else
// art::ArtMethod::CopyFrom(art::ArtMethod*, unsigned int)
SYMBOL kArtMethod_CopyFrom_N = "_ZN3art9ArtMethod8CopyFromEPS0_j";
// art::ArtMethod::CopyFrom(art::ArtMethod const*, unsigned int)
SYMBOL kArtMethod_CopyFrom_M = "_ZN3art9ArtMethod8CopyFromEPKS0_j";
#endif

// art::GetMethodShorty(_JNIEnv*, _jmethodID*)
SYMBOL kArt_GetMethodShorty = "_ZN3artL15GetMethodShortyEP7_JNIEnvP10_jmethodID";
SYMBOL kArt_GetMethodShorty_Legacy = "_ZN3art15GetMethodShortyEP7_JNIEnvP10_jmethodID";

// art::Dbg::SuspendVM()
SYMBOL kDbg_SuspendVM = "_ZN3art3Dbg9SuspendVMEv";
// art::Dbg::ResumeVM()
SYMBOL kDbg_ResumeVM = "_ZN3art3Dbg8ResumeVMEv";

// art_quick_to_interpreter_bridge()
SYMBOL kArt_art_quick_to_interpreter_bridge = "art_quick_to_interpreter_bridge";

// artInterpreterToCompiledCodeBridge
SYMBOL kArt_artInterpreterToCompiledCodeBridge = "artInterpreterToCompiledCodeBridge";

// art::ProfileSaver::ForceProcessProfiles()
SYMBOL kArt_ProfileSaver_ForceProcessProfiles = "_ZN3art12ProfileSaver20ForceProcessProfilesEv";

#if defined(__LP64__)
// art::LinearAlloc::Alloc(art::Thread*, unsigned int)
SYMBOL kArt_LinearAlloc_Alloc = "_ZN3art11LinearAlloc5AllocEPNS_6ThreadEm";
#else
SYMBOL kArt_LinearAlloc_Alloc = "_ZN3art11LinearAlloc5AllocEPNS_6ThreadEj";
#endif
// art::Thread::DecodeJObject(_jobject*) const
SYMBOL kArt_DecodeJObject = "_ZNK3art6Thread13DecodeJObjectEP8_jobject";

// art::ClassLinker::EnsureInitialized(art::Thread*, art::Handle<art::mirror::Class>, bool, bool)
SYMBOL kArt_EnsureInitialized = "_ZN3art11ClassLinker17EnsureInitializedEPNS_6ThreadENS_6HandleINS_6mirror5ClassEEEbb";

// art::mirror::Object::Clone(art::Thread*)
SYMBOL kArt_Object_Clone = "_ZN3art6mirror6Object5CloneEPNS_6ThreadE";

// art::mirror::Object::Clone(art::Thread*, art::mirror::Class*)
SYMBOL kArt_Object_CloneWithClass = "_ZN3art6mirror6Object5CloneEPNS_6ThreadEPNS0_5ClassE";

#if defined(__LP64__)
// art::mirror::Object::Clone(art::Thread*, unsigned long)
SYMBOL kArt_Object_CloneWithSize = "_ZN3art6mirror6Object5CloneEPNS_6ThreadEm";
#else
// art::mirror::Object::Clone(art::Thread*, unsigned int)
SYMBOL kArt_Object_CloneWithSize = "_ZN3art6mirror6Object5CloneEPNS_6ThreadEj";
#endif

SYMBOL kArt_JniEnvExt_NewLocalRef = "_ZN3art9JNIEnvExt11NewLocalRefEPNS_6mirror6ObjectE";

//symbol 符号名字  _ZN3artL15GetMethodShortyEP7_JNIEnvP10_jmethodID
//decl   被赋值的函数指针
//ret    目前看来都是 测试使用 false  应该是表示是否是必备函数
#define FIND_SYMBOL(symbol, decl, ret)  \
        if ((decl = reinterpret_cast<typeof(decl)>(WDynamicLibSymbol(elf_image, symbol))) == nullptr) {  \
            if (ret) {  \
                LOG(ERROR) << "Failed to resolve symbol : " << #symbol;  \
                return false;  \
             } \
        }
        // 拆分
        // ① WDynamicLibSymbol(elf_image, symbol) 根据so的开始地址找到对应函数的地址
        // ② decl = reinterpret_cast<typeof(decl)>(WDynamicLibSymbol(elf_image, symbol) 判断类型是否相同 赋值给函数指针
        // ③最后判断 是否为Null

bool ArtSymbolResolver::Resolve(void *elf_image, s4 api_level) {
    // dex_file.cc
    // 根据一个methodID返回一个 class的类型  类似 这种 void amap.WalkRouteActivity.showPop()
    FIND_SYMBOL(kArt_GetMethodShorty, symbols_.Art_GetMethodShorty, false);
    if (symbols_.Art_GetMethodShorty == nullptr) {
        FIND_SYMBOL(kArt_GetMethodShorty_Legacy, symbols_.Art_GetMethodShorty, false);
    }
    //小于7.0
    if (api_level < ANDROID_N) {
        //interpreter_common.cc
        //artInterpreterToCompiledCodeBridge
        //让一个解释执行的类方法跳到另一个以本地机器指令执行的类方法去执行。返回类型是 void
        //解释执行变二进制执行  因为 Art虚拟机 7.0版本是 及时JIT 所以可能某个方法 没有被JIT
        //6.0以上的安装包 会在 安装的时候进行JIT 安装速度很慢 所以只有7.0一下 可以让任意方法进行二进制执行
        FIND_SYMBOL(kArt_artInterpreterToCompiledCodeBridge,
                    symbols_.artInterpreterToCompiledCodeBridge, false);
    }

    //暂停虚拟机的 主要用处是被Hook方法已经是Native方法 虚拟机用处不大
    //SuspendVM 和 fast_jni相关 ，防止虚拟机和本地二进制执行 切换 浪费时间
    //和GC也有关系防止被回收
    FIND_SYMBOL(kDbg_SuspendVM, symbols_.Dbg_SuspendVM, false);
    //开始
    FIND_SYMBOL(kDbg_ResumeVM, symbols_.Dbg_ResumeVM, false);

    //让某个方法 从字节码执行变成解释执行
    FIND_SYMBOL(kArt_art_quick_to_interpreter_bridge, symbols_.art_quick_to_interpreter_bridge,
                false);

    //7.0 版本以上的方法取消jit编译的 方法
    //void ProfileSaver::ForceProcessProfiles() 返回的是一个 jit编译的配置表
    //因为 将被Hook方法变成Native以后  就需要 取消 全部的 优化 比如 jit编译
    //虚拟机 运行状态之类的
    if (api_level > ANDROID_N) {
        FIND_SYMBOL(kArt_ProfileSaver_ForceProcessProfiles,
                    symbols_.ProfileSaver_ForceProcessProfiles,
                    false);
    }
    if (api_level > ANDROID_O) {
        // 8.0特有方法 给当前 ArtMethod设置所属类
        //判断 当前方法 是否已经 JIT 是否为Native方法 将 Hotness_count_ 设置为0
        FIND_SYMBOL(kArtMethod_CopyFrom_O, symbols_.ArtMethod_CopyFrom, false);
    } else if (api_level > ANDROID_N) {
        FIND_SYMBOL(kArtMethod_CopyFrom_N, symbols_.ArtMethod_CopyFrom, false);
    } else {
        FIND_SYMBOL(kArtMethod_CopyFrom_M, symbols_.ArtMethod_CopyFrom, false);
    }

    FIND_SYMBOL(kArt_Object_Clone, symbols_.Object_Clone, false);
    if (symbols_.Object_Clone == nullptr) {
        FIND_SYMBOL(kArt_Object_CloneWithSize, symbols_.Object_CloneWithSize, false);
    }
    if (symbols_.Object_Clone == nullptr) {
        FIND_SYMBOL(kArt_Object_CloneWithClass, symbols_.Object_CloneWithClass, true);
    }
    FIND_SYMBOL(kArt_DecodeJObject, symbols_.Thread_DecodeJObject, true);
    FIND_SYMBOL(kArt_JniEnvExt_NewLocalRef, symbols_.JniEnvExt_NewLocalRef, true);
    return true;
#undef FIND_SYMBOL
}

}  // namespace art
}  // namespace whale
