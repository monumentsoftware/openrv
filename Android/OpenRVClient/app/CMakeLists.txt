# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

find_library(log-lib log)

# NOTE: The android toolchain file ignores PATHS and HINTS parameters of find_package(), likewise it
# ignores CMAKE_PREFIX_PATH and PackageName_DIR (despite the misleading error message from
# find_package()).
# instead, we need to add the search path for the packagename-config.cmake file to the
# CMAKE_FIND_ROOT_PATH.
#list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../")
# TODO: add cmake module for libopenrv so we can use find_package()
set(LIBOPENRV_PATH ${CMAKE_SOURCE_DIR}/../../openrv)
set(LIBOPENRV_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/../../../libopenrv/public
    ${CMAKE_SOURCE_DIR}/../../../libopenrv/public/libopenrv
    # TODO: cmake variable for build variant? debug/release/etc.? (CMAKE_BUILD_TYPE is probably not
    # correct, I believe we actually want the android build variant)
    ${LIBOPENRV_PATH}/libopenrv/.externalNativeBuild/cmake/debug/${ANDROID_ABI}/openrv/libopenrv/public
    ${LIBOPENRV_PATH}/libopenrv/.externalNativeBuild/cmake/debug/${ANDROID_ABI}/openrv/libopenrv/public/libopenrv
)
set(LIBOPENRV_LIBRARIES ${LIBOPENRV_PATH}/libopenrv/build/intermediates/cmake/debug/obj/${ANDROID_ABI}/libopenrv_android.so)


set(openrvclient_srcs
    src/main/cpp/openrvclient_jni.cpp
)

add_library(openrvclient
    SHARED
    ${openrvclient_srcs}
)
target_include_directories(openrvclient
    PUBLIC
    ${LIBOPENRV_INCLUDE_DIRECTORIES}
)
target_link_libraries(openrvclient
    ${log-lib}
    ${LIBOPENRV_LIBRARIES}
)

