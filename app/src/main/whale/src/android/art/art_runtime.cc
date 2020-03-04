#include <dlfcn.h>
#include "whale.h"
#include "android/android_build.h"
#include "android/art/art_runtime.h"
#include "android/art/modifiers.h"
#include "android/art/native_on_load.h"
#include "android/art/art_method.h"
#include "android/art/art_symbol_resolver.h"
#include "android/art/scoped_thread_state_change.h"
#include "android/art/art_jni_trampoline.h"
#include "android/art/well_known_classes.h"
#include "android/art/java_types.h"
#include "platform/memory.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/cxx_helper.h"


#define CHECK_FIELD(field, value)  \
    if ((field) == (value)) {  \
        LOG(ERROR) << "Failed to find " #field ".";  \
        return false;  \
    }


namespace whale {
    namespace art {

        ArtRuntime *ArtRuntime::Get() {
            static ArtRuntime instance;
            return &instance;
        }

//加载需要的class 一些
        void PreLoadRequiredStuff(JNIEnv *env) {
            //Java层常用的类型
            Types::Load(env);
            //Java常用的类
            WellKnownClasses::Load(env);
            //
            ScopedNoGCDaemons::Load(env);
        }


        bool ArtRuntime::OnLoad(JavaVM *vm, JNIEnv *env, jclass java_class) {
            LOG(ERROR) << "开始执行 art_runtime  OnLoad  函数";


            //判断 libhoudini.so 是否加载到内存里
            //这个so主要判断 是否是模拟器的 默认是Arm处理器
            //在模拟器里面 libhoudini.so 含有这个so 正常Arm是没有的 IsFileInMemory Arm64位手机返回的是 0 false

            //    (apk中有对应平台的文件夹，如果项目只包含了 armeabi，那么在所有Android设备都可以运行； 
            //    如果项目只包含了 armeabi-v7a，除armeabi架构的设备外都可以运行； 如果项目只包含了 x86，
            //    那么armeabi架构和armeabi-v7a的Android设备是无法运行的；

            if ((kRuntimeISA == InstructionSet::kArm || kRuntimeISA == InstructionSet::kArm64)
                && IsFileInMemory("libhoudini.so")) {
                LOG(INFO) << '[' << getpid() << ']' << " Unable to launch on houdini environment.";
                return false;
            }
            //这块 设置全局的 vm
            vm_ = vm;

            //这块相当于把这个 class设置成全局的 防止被回收
            java_class_ = reinterpret_cast<jclass>(env->NewGlobalRef(java_class));

            //拿到 handleHookedMethod 的MethodID这个方法是 处理被Hook方法的 参数信息，返回值之类的
            bridge_method_ = env->GetStaticMethodID(
                    java_class,
                    "handleHookedMethod",
                    "(Ljava/lang/reflect/Member;JLjava/lang/Object;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;"
            );



            if (JNIExceptionCheck(env)) {
                //发现异常直接 返回 false
                return false;
            }

            //拿到安卓版本号
            api_level_ = GetAndroidApiLevel();

            //加载需要的东西 主要是FindClass虚拟机里面的一些常用方法
            PreLoadRequiredStuff(env);

            const char *art_path = kLibArtPath;
            //这块相当于拿到 "/system/lib/libart.so"的开始地址 通过解析Map文件
            art_elf_image_ = WDynamicLibOpen(art_path);


            if (art_elf_image_ == nullptr) {
                LOG(ERROR) << "Unable to read data from libart.so.";
                return false;
            }

            //这块算是对版本 兼容 进行是否匹配的判断
            //主要是从Art.so里面拿到 各种函数  方便后面操作
            if (!art_symbol_resolver_.Resolve(art_elf_image_, api_level_)) {
                // The log will all output from ArtSymbolResolver.
                return false;
            }

            //JNI的偏移
            offset_t jni_code_offset = INT32_MAX;
            //这个是 方法FLAG得 偏移
            offset_t access_flags_offset = INT32_MAX;

            //需要兼容 判断指针大小
            size_t entrypoint_filed_size = (api_level_ <= ANDROID_LOLLIPOP) ? 8 : kPointerSize;

            LOG(ERROR) << "entrypoint_filed_size 指针大小   函数  " << entrypoint_filed_size;

            //生成一个 FLAG 是 private  Static  Ntive  方法
            u4 expected_access_flags = kAccPrivate | kAccStatic | kAccNative;

            //java_class  是java层的 WhaleRuntime
            jmethodID reserved0 = env->GetStaticMethodID(java_class, kMethodReserved0, "()V");

            jmethodID reserved1 = env->GetStaticMethodID(java_class, kMethodReserved1, "()V");


            //for每一次 加上 四个字节的偏移
            for (offset_t offset = 0; offset != sizeof(u4) * 24; offset += sizeof(u4)) {
                //遍历 找到类型 为 expected_access_flags 的变量
                if (MemberOf<u4>(reserved0, offset) == expected_access_flags) {
                    access_flags_offset = offset;
                    break;
                }
            }


            //拿到 函数入口  函数指针 转换成     void * 类型
            void *native_function = reinterpret_cast<void *>(WhaleRuntime_reserved0);
            //这个地方是查找 jni的 偏移
            for (offset_t offset = 0; offset != sizeof(u4) * 24; offset += sizeof(u4)) {
                if (MemberOf<ptr_t>(reserved0, offset) == native_function) {
                    //这块相当于找到某个 Native 函数的地址
                    jni_code_offset = offset;
                    break;
                }
            }


            // 判断 是否初始化成功 也就是 是否查找到 并且赋值
            CHECK_FIELD(access_flags_offset, INT32_MAX)
            CHECK_FIELD(jni_code_offset, INT32_MAX)

            //每个 Method的大小 根据两个 Native 方法 找到间隔大小
            method_offset_.method_size_ = DistanceOf(reserved0, reserved1);
            // 放在 method_offset_ 进行保存
            method_offset_.jni_code_offset_ = jni_code_offset;
            method_offset_.quick_code_offset_ = jni_code_offset + entrypoint_filed_size;
            method_offset_.access_flags_offset_ = access_flags_offset;
            method_offset_.dex_code_item_offset_offset_ = access_flags_offset + sizeof(u4);
            method_offset_.dex_method_index_offset_ = access_flags_offset + sizeof(u4) * 2;
            method_offset_.method_index_offset_ = access_flags_offset + sizeof(u4) * 3;


            //小于7.0 兼容 安卓7.0 以下版本
            if (api_level_ < ANDROID_N &&
                GetSymbols()->artInterpreterToCompiledCodeBridge != nullptr) {
                // 判断 是否 小于7.0  这个方法的 artInterpreterToCompiledCodeBridge 的功能是解释执行变成二进制执行
                // 7.0 以上是自动进行JIT的  而不是安装时候 自动JIT所以 存在可能 某个方法没有被JIT 过
                // 所以 这个方法7.0以上没有
                LOG(ERROR) << "是否执行赋值   interpreter_code_offset_";
                method_offset_.interpreter_code_offset_ = jni_code_offset - entrypoint_filed_size;
            }
            if (api_level_ >= ANDROID_N) {
                LOG(ERROR) << "是否执行赋值   hotness_count_offset_";
                //7.0 以上特有字段 一个Hot值 跟JIT 相关 每一次调用都会增加 超过 某个数值 方法会自动进行JIT
                method_offset_.hotness_count_offset_ =
                        method_offset_.method_index_offset_ + sizeof(u2);
            }
            //找到 art_quick_generic_jni_trampoline 这个函数地址
            ptr_t quick_generic_jni_trampoline = WDynamicLibSymbol(
                    art_elf_image_,
                    "art_quick_generic_jni_trampoline"
            );

            env->CallStaticVoidMethod(java_class, reserved0);

            /**
             * Fallback to do a relative memory search for quick_generic_jni_trampoline,
             * This case is almost impossible to enter
             * because its symbols are found almost always on all devices.
             * This algorithm has been verified on 5.0 ~ 9.0.
             * And we're pretty sure that its structure has not changed in the OEM Rom.
             */
            if (quick_generic_jni_trampoline == nullptr) {
                LOG(ERROR) << "是否执行   quick_generic_jni_trampoline == nullptr";

                ptr_t heap = nullptr;
                ptr_t thread_list = nullptr;
                ptr_t class_linker = nullptr;
                ptr_t intern_table = nullptr;

                ptr_t runtime = MemberOf<ptr_t>(vm, kPointerSize);
                CHECK_FIELD(runtime, nullptr)
                runtime_objects_.runtime_ = runtime;

                offset_t start = (kPointerSize == 4) ? 200 : 384;
                offset_t end = start + (100 * kPointerSize);
                for (offset_t offset = start; offset != end; offset += kPointerSize) {
                    if (MemberOf<ptr_t>(runtime, offset) == vm) {
                        size_t class_linker_offset =
                                offset - (kPointerSize * 3) - (2 * kPointerSize);
                        if (api_level_ >= ANDROID_O_MR1) {
                            class_linker_offset -= kPointerSize;
                        }
                        offset_t intern_table_offset = class_linker_offset - kPointerSize;
                        offset_t thread_list_Offset = intern_table_offset - kPointerSize;
                        offset_t heap_offset = thread_list_Offset - (4 * kPointerSize);
                        if (api_level_ >= ANDROID_M) {
                            heap_offset -= 3 * kPointerSize;
                        }
                        if (api_level_ >= ANDROID_N) {
                            heap_offset -= kPointerSize;
                        }
                        heap = MemberOf<ptr_t>(runtime, heap_offset);
                        thread_list = MemberOf<ptr_t>(runtime, thread_list_Offset);
                        class_linker = MemberOf<ptr_t>(runtime, class_linker_offset);
                        intern_table = MemberOf<ptr_t>(runtime, intern_table_offset);
                        break;
                    }
                }
                CHECK_FIELD(heap, nullptr)
                CHECK_FIELD(thread_list, nullptr)
                CHECK_FIELD(class_linker, nullptr)
                CHECK_FIELD(intern_table, nullptr)

                runtime_objects_.heap_ = heap;
                runtime_objects_.thread_list_ = thread_list;
                runtime_objects_.class_linker_ = class_linker;
                runtime_objects_.intern_table_ = intern_table;

                start = kPointerSize * 25;
                end = start + (100 * kPointerSize);
                for (offset_t offset = start; offset != end; offset += kPointerSize) {
                    if (MemberOf<ptr_t>(class_linker, offset) == intern_table) {
                        offset_t target_offset =
                                offset + ((api_level_ >= ANDROID_M) ? 3 : 5) * kPointerSize;
                        quick_generic_jni_trampoline = MemberOf<ptr_t>(class_linker, target_offset);
                        break;
                    }
                }
            }
            CHECK_FIELD(quick_generic_jni_trampoline, nullptr)
            class_linker_objects_.quick_generic_jni_trampoline_ = quick_generic_jni_trampoline;

            pthread_mutex_init(&mutex, nullptr);
            EnforceDisableHiddenAPIPolicy();
            if (api_level_ >= ANDROID_N) {
                FixBugN();
            }
            return true;

#undef CHECK_OFFSET
        }

        jlong
        ArtRuntime::HookMethod(JNIEnv *env, jclass decl_class, jobject hooked_java_method,
                               jobject addition_info) {

            ScopedSuspendAll suspend_all;
            //转换一个java.lang.reflect.Method或java.lang.reflect.Constructor对象到一个方法ID。
            //被Hook的方法
            jmethodID hooked_jni_method = env->FromReflectedMethod(hooked_java_method);


            //根据方法ID 拿到 对应的方法ArtMethod
            ArtMethod hooked_method(hooked_jni_method);
            //保存了Hook方法的信息
            auto *param = new ArtHookParam();

            //设置当前类的Classloader 需要设置成 不可回收的全局变量
            param->class_Loader_ = env->NewGlobalRef(
                    env->CallObjectMethod(
                            // 用所属类的 class 调用 里面的 getClassloader方法
                            decl_class,
                            WellKnownClasses::java_lang_Class_getClassLoader
                    )
            );
            //设置当前方法的描述 被Hook方法的
            param->shorty_ = hooked_method.GetShorty(env, hooked_java_method);
            //判断是否是静态的
            param->is_static_ = hooked_method.HasAccessFlags(kAccStatic);
            //获取二进制入口
            param->origin_compiled_code_ = hooked_method.GetEntryPointFromQuickCompiledCode();
            //获取codeItem的 偏移
            param->origin_code_item_off = hooked_method.GetDexCodeItemOffset();
            // JNI的入口 每个沙盒都有自己的 JNI入口
            param->origin_jni_code_ = hooked_method.GetEntryPointFromJni();
            //设置方法的 flag
            param->origin_access_flags = hooked_method.GetAccessFlags();
            //克隆一个Java层的 Method 这个是被Hook以后 会走的方法（替换的方法）
            jobject origin_java_method = hooked_method.Clone(env, param->origin_access_flags);

            ResolvedSymbols *symbols = GetSymbols();
            //取消JIT编译 这个方法返回的JIT的一个配置表
            if (symbols->ProfileSaver_ForceProcessProfiles) {
                symbols->ProfileSaver_ForceProcessProfiles();
            }

            // After android P, hotness_count_ maybe an imt_index_ for abstract method
            if ((api_level_ > ANDROID_P && !hooked_method.HasAccessFlags(kAccAbstract))
                || api_level_ >= ANDROID_N) {
                //判断 如果不是抽象方法的时候 在将 Jit里面的 HotnessCount 设置成 0
                hooked_method.SetHotnessCount(0);
            }

            // Clear the dex_code_item_offset_.
            // It needs to be 0 since hooked methods have no CodeItems but the
            // method they copy might.
            // 将dexCodeItem的 偏移 设置成 0 因为希望方法变成native方法
            // 因为 需要将原方法变成Native 就需要取消JIT编译，去掉Java方法的特性
            hooked_method.SetDexCodeItemOffset(0);

            //判断 并且获取 Flag(是否是Native )
            u4 access_flags = hooked_method.GetAccessFlags();
            if (api_level_ < ANDROID_O_MR1) {
                access_flags |= kAccCompileDontBother_N;
            } else {
                access_flags |= kAccCompileDontBother_O_MR1;
                access_flags |= kAccPreviouslyWarm_O_MR1;
            }

            access_flags |= kAccNative;
            access_flags |= kAccFastNative;
            if (api_level_ >= ANDROID_P) {
                access_flags &= ~kAccCriticalNative_P;
            }
            //设置成native
            hooked_method.SetAccessFlags(access_flags);
            //设置Native入口
            hooked_method.SetEntryPointFromQuickCompiledCode(
                    class_linker_objects_.quick_generic_jni_trampoline_
            );

            //7.0一下会安装时候 自动JIT，所以可以将某个方法设置成解释执行切换成二进制执行。
            //高版本JIT是及时性的，所以没有这个方法
            if (api_level_ < ANDROID_N
                && symbols->artInterpreterToCompiledCodeBridge != nullptr) {
                //让解释执行变成 二进制执行 并设置入口
                hooked_method.SetEntryPointFromInterpreterCode(
                        symbols->artInterpreterToCompiledCodeBridge);
            }

            //将复制成功的方法转换成methodID（替换被Hook的方法）
            //转换一个java.lang.reflect.Method或java.lang.reflect.Constructor对象到一个方法ID。
            param->origin_native_method_ = env->FromReflectedMethod(origin_java_method);
            //被Hook的方法 这个方法已经变成native
            param->hooked_native_method_ = hooked_jni_method;
            //Java层 传入的全局变量
            param->addition_info_ = env->NewGlobalRef(addition_info);
            //被Hook方法的 Method
            param->hooked_method_ = env->NewGlobalRef(hooked_java_method);
            //替换的 方法的 Method
            param->origin_method_ = env->NewGlobalRef(origin_java_method);

            //    ArtRuntime::HookMethod将被Hook的method设置为native方法，然后将Jni入口点设置为一个closure。这样当该method被调用时，
            //    就会执行这里设置的closure。因为被Hook的method已经为native，所以ArtMethod结构中的dex_code_item_offset_成员就没用了，
            //    直接清0。
            //
            //    另外，将quick_compiled_code和interpreter的入口点分别设置为quick_generic_jni_trampoline_
            //    和artInterpreterToCompiledCodeBridge，这样无论被Hook的方法从解释器执行，
            //    还是直接以本地指令的方式执行，最终都会执行Jni入口点，都会执行这里设置的closure。
            //
            //    所以，ArtRuntime::HookMethod执行之后，被Hook方法的执行就由closure接管了，这个closure是由BuildJniClosure构造的
            BuildJniClosure(param);

            //将被Hook方法的 fnPtr赋值 (Native 入口 最终入口 )
            //这个方法 SetEntryPointFromJni  也是注册Native方法必走的方法
            // 本质是将ArtMethod里面的 fnptr赋值的过程
            hooked_method.SetEntryPointFromJni(param->jni_closure_->GetCode());
            //所属Class
            param->decl_class_ = hooked_method.GetDeclaringClass();
            //将被Hook的方法放进去
            //make_pair将两个类型合并成一个 make_pair 不需要使用泛型 自动判断类型
            //pair 需要使用泛型
            hooked_method_map_.insert(std::make_pair(hooked_jni_method, param));

            return reinterpret_cast<jlong>(param);
        }

        //这个方法是调用被Hook的替换方法的
        jobject
        ArtRuntime::InvokeOriginalMethod(jlong slot, jobject this_object, jobjectArray args) {

            JNIEnv *env = GetJniEnv();

            if (slot <= 0) {
                env->ThrowNew(
                        WellKnownClasses::java_lang_IllegalArgumentException,
                        "Failed to resolve slot."
                );
                return nullptr;
            }

            //在Runtime里面 slot表示了 方法的信息
            auto *param = reinterpret_cast<ArtHookParam *>(slot);
            //将 被Hook方法 转换成 ArtMethod
            ArtMethod hooked_method(param->hooked_native_method_);
            //获取所属类的指针
            ptr_t decl_class = hooked_method.GetDeclaringClass();

            if (param->decl_class_ != decl_class) {
                pthread_mutex_lock(&mutex);
                //这块进行判断 如果所属的class不匹配的话需要重新进行赋值
                if (param->decl_class_ != decl_class) {
                    ScopedSuspendAll suspend_all;
                    LOG(INFO)
                            << "Notice: MovingGC cause the GcRoot References changed.";
                    jobject origin_java_method = hooked_method.Clone(env,
                                                                     param->origin_access_flags);
                    jmethodID origin_jni_method = env->FromReflectedMethod(origin_java_method);
                    ArtMethod origin_method(origin_jni_method);
                    //将ArtMethod进行重新赋值 以param为主
                    origin_method.SetEntryPointFromQuickCompiledCode(param->origin_compiled_code_);
                    origin_method.SetEntryPointFromJni(param->origin_jni_code_);
                    origin_method.SetDexCodeItemOffset(param->origin_code_item_off);
                    param->origin_native_method_ = origin_jni_method;
                    env->DeleteGlobalRef(param->origin_method_);
                    param->origin_method_ = env->NewGlobalRef(origin_java_method);
                    param->decl_class_ = decl_class;
                }
                pthread_mutex_unlock(&mutex);
            }
            //调用抽象方法的
            //本质相当于反射调用 Method里面的 invoke
            jobject ret = env->CallNonvirtualObjectMethod(
                    param->origin_method_,
                    WellKnownClasses::java_lang_reflect_Method,
                    WellKnownClasses::java_lang_reflect_Method_invoke,
                    this_object,
                    args
            );
            return ret;
        }

#if defined(__aarch64__)
# define __get_tls() ({ void** __val; __asm__("mrs %0, tpidr_el0" : "=r"(__val)); __val; })
#elif defined(__arm__)
# define __get_tls() ({ void** __val; __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(__val)); __val; })
#elif defined(__i386__)
# define __get_tls() ({ void** __val; __asm__("movl %%gs:0, %0" : "=r"(__val)); __val; })
#elif defined(__x86_64__)
# define __get_tls() ({ void** __val; __asm__("mov %%fs:0, %0" : "=r"(__val)); __val; })
#else
#error unsupported architecture
#endif
        //本质获取Java层的 currentThread
        ArtThread *ArtRuntime::GetCurrentArtThread() {
            if (WellKnownClasses::java_lang_Thread_nativePeer) {
                JNIEnv *env = GetJniEnv();
                jobject current = env->CallStaticObjectMethod(
                        WellKnownClasses::java_lang_Thread,
                        WellKnownClasses::java_lang_Thread_currentThread
                );
                return reinterpret_cast<ArtThread *>(
                        env->GetLongField(current, WellKnownClasses::java_lang_Thread_nativePeer)
                );
            }
            return reinterpret_cast<ArtThread *>(__get_tls()[7/*TLS_SLOT_ART_THREAD_SELF*/]);
        }

        jobject
        ArtRuntime::InvokeHookedMethodBridge(JNIEnv *env, ArtHookParam *param, jobject receiver,
                                             jobjectArray array) {
            return env->CallStaticObjectMethod(java_class_, bridge_method_,
                                               param->hooked_method_,
                                               reinterpret_cast<jlong>(param),
                                               param->addition_info_, receiver, array);
        }
        //根据一个 class 和 Method 返回一个slot对象
        jlong ArtRuntime::GetMethodSlot(JNIEnv *env, jclass cl, jobject method_obj) {
            if (method_obj == nullptr) {
                env->ThrowNew(
                        WellKnownClasses::java_lang_IllegalArgumentException,
                        "Method param == null"
                );
                return 0;
            }
            //将Method转换成 jmethod
            jmethodID jni_method = env->FromReflectedMethod(method_obj);

            auto entry = hooked_method_map_.find(jni_method);

            if (entry == hooked_method_map_.end()) {
                env->ThrowNew(
                        WellKnownClasses::java_lang_IllegalArgumentException,
                        "Failed to find slot."
                );
                return 0;
            }
            return reinterpret_cast<jlong>(entry->second);
        }

        void ArtRuntime::EnsureClassInitialized(JNIEnv *env, jclass cl) {
            // This invocation will ensure the target class has been initialized also.
            ScopedLocalRef<jobject> unused(env, env->AllocObject(cl));
            JNIExceptionClear(env);
        }

        void ArtRuntime::SetObjectClass(JNIEnv *env, jobject obj, jclass cl) {
            SetObjectClassUnsafe(env, obj, cl);
        }

        void ArtRuntime::SetObjectClassUnsafe(JNIEnv *env, jobject obj, jclass cl) {
            jfieldID java_lang_Class_shadow$_klass_ = env->GetFieldID(
                    WellKnownClasses::java_lang_Object,
                    "shadow$_klass_",
                    "Ljava/lang/Class;"
            );
            env->SetObjectField(obj, java_lang_Class_shadow$_klass_, cl);
        }

        jobject ArtRuntime::CloneToSubclass(JNIEnv *env, jobject obj, jclass sub_class) {
            ResolvedSymbols *symbols = GetSymbols();
            ArtThread *thread = GetCurrentArtThread();
            ptr_t art_object = symbols->Thread_DecodeJObject(thread, obj);
            ptr_t art_clone_object = CloneArtObject(art_object);
            jobject clone = symbols->JniEnvExt_NewLocalRef(env, art_clone_object);
            SetObjectClassUnsafe(env, clone, sub_class);
            return clone;
        }

        void ArtRuntime::RemoveFinalFlag(JNIEnv *env, jclass java_class) {
            jfieldID java_lang_Class_accessFlags = env->GetFieldID(
                    WellKnownClasses::java_lang_Class,
                    "accessFlags",
                    "I"
            );
            jint access_flags = env->GetIntField(java_class, java_lang_Class_accessFlags);
            env->SetIntField(java_class, java_lang_Class_accessFlags, access_flags & ~kAccFinal);
        }

        bool ArtRuntime::EnforceDisableHiddenAPIPolicy() {
            if (GetAndroidApiLevel() < ANDROID_O_MR1) {
                return true;
            }
            static Singleton<bool> enforced([&](bool *result) {
                *result = EnforceDisableHiddenAPIPolicyImpl();
            });
            return enforced.Get();
        }

        bool OnInvokeHiddenAPI() {
            return false;
        }

        /**
         * NOTICE:
         * After Android Q(10.0), GetMemberActionImpl has been renamed to ShouldDenyAccessToMemberImpl,
         * But we don't know the symbols until it's published.
         */
        ALWAYS_INLINE bool ArtRuntime::EnforceDisableHiddenAPIPolicyImpl() {
            JNIEnv *env = GetJniEnv();
            jfieldID java_lang_Class_shadow$_klass_ = env->GetFieldID(
                    WellKnownClasses::java_lang_Object,
                    "shadow$_klass_",
                    "Ljava/lang/Class;"
            );
            JNIExceptionClear(env);
            if (java_lang_Class_shadow$_klass_ != nullptr) {
                return true;
            }
            void *symbol = nullptr;

            // Android P : Preview 1 ~ 4 version
            symbol = WDynamicLibSymbol(
                    art_elf_image_,
                    "_ZN3art9hiddenapi25ShouldBlockAccessToMemberINS_8ArtFieldEEEbPT_PNS_6ThreadENSt3__18functionIFbS6_EEENS0_12AccessMethodE"
            );
            if (symbol) {
                WInlineHookFunction(symbol, reinterpret_cast<void *>(OnInvokeHiddenAPI), nullptr);
            }
            symbol = WDynamicLibSymbol(
                    art_elf_image_,
                    "_ZN3art9hiddenapi25ShouldBlockAccessToMemberINS_9ArtMethodEEEbPT_PNS_6ThreadENSt3__18functionIFbS6_EEENS0_12AccessMethodE"
            );

            if (symbol) {
                WInlineHookFunction(symbol, reinterpret_cast<void *>(OnInvokeHiddenAPI), nullptr);
                return true;
            }
            // Android P : Release version
            symbol = WDynamicLibSymbol(
                    art_elf_image_,
                    "_ZN3art9hiddenapi6detail19GetMemberActionImplINS_8ArtFieldEEENS0_6ActionEPT_NS_20HiddenApiAccessFlags7ApiListES4_NS0_12AccessMethodE"
            );
            if (symbol) {
                WInlineHookFunction(symbol, reinterpret_cast<void *>(OnInvokeHiddenAPI), nullptr);
            }
            symbol = WDynamicLibSymbol(
                    art_elf_image_,
                    "_ZN3art9hiddenapi6detail19GetMemberActionImplINS_9ArtMethodEEENS0_6ActionEPT_NS_20HiddenApiAccessFlags7ApiListES4_NS0_12AccessMethodE"
            );
            if (symbol) {
                WInlineHookFunction(symbol, reinterpret_cast<void *>(OnInvokeHiddenAPI), nullptr);
            }
            return symbol != nullptr;
        }

        //针对 6.0一下版本的 Clone函数 将一个 jmethodID 返回成指针
        //可以reinterpret_cast<jmethodID>  强转换成    jmethodID
        ptr_t ArtRuntime::CloneArtObject(ptr_t art_object) {
            ResolvedSymbols *symbols = GetSymbols();
            if (symbols->Object_Clone) {
                return symbols->Object_Clone(art_object, GetCurrentArtThread());
            }
            if (symbols->Object_CloneWithClass) {
                return symbols->Object_CloneWithClass(art_object, GetCurrentArtThread(), nullptr);
            }
            return symbols->Object_CloneWithSize(art_object, GetCurrentArtThread(), 0);
        }

        int (*old_ToDexPc)(void *thiz, void *a2, unsigned int a3, int a4);

        int new_ToDexPc(void *thiz, void *a2, unsigned int a3, int a4) {
            return old_ToDexPc(thiz, a2, a3, 0);
        }

        bool is_hooked = false;

        void ArtRuntime::FixBugN() {
            if (is_hooked)
                return;
            void *symbol = nullptr;
            symbol = WDynamicLibSymbol(
                    art_elf_image_,
                    "_ZNK3art20OatQuickMethodHeader7ToDexPcEPNS_9ArtMethodEjb"
            );
            if (symbol) {
                WInlineHookFunction(symbol, reinterpret_cast<void *>(new_ToDexPc),
                                    reinterpret_cast<void **>(&old_ToDexPc));
            }
            is_hooked = true;
        }

    }  // namespace art
}  // namespace whale
