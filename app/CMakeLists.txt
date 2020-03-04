
cmake_minimum_required(VERSION 3.4.1)


#2020年2月25日16:51:29
#添加子目录，将自动找到子目录中的CMakeLists.txt
ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/src/main/whale)


#ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/src/main/xposed/Android.mk)
#用于设定目录，这些设定的目录将被编译器用来查找 include 文件
#include_directories(${PROJECT_SOURCE_DIR}/src/main/xposed/)