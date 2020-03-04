#ifndef WHALE_ANDROID_ANDROID_BUILD_H_
#define WHALE_ANDROID_ANDROID_BUILD_H_

#include <cstdint>
#include <cstdlib>
#include <sys/system_properties.h>

#define ANDROID_ICE_CREAM_SANDWICH      14
#define ANDROID_ICE_CREAM_SANDWICH_MR1  15
#define ANDROID_JELLY_BEAN              16
#define ANDROID_JELLY_BEAN_MR1          17
#define ANDROID_JELLY_BEAN_MR2          18
#define ANDROID_KITKAT                  19
#define ANDROID_KITKAT_WATCH            20

#define ANDROID_LOLLIPOP                21   //5.1
#define ANDROID_LOLLIPOP_MR1            22
#define ANDROID_M                       23    //6.0

#define ANDROID_N                       24    //7.0

#define ANDROID_N_MR1                   25   //7.1
#define ANDROID_O                       26   //8.0
#define ANDROID_O_MR1                   27   //8.1

#define ANDROID_P                       28   //9.0

static inline int32_t GetAndroidApiLevel() {
    //通过build变量拿到版本号
    char prop_value[PROP_VALUE_MAX];
    __system_property_get("ro.build.version.sdk", prop_value);
    return atoi(prop_value);
}

#endif  // WHALE_ANDROID_ANDROID_BUILD_H_
