#
# Android: When building for android, call cmake using -DBUILD_ANDROID=YES when calling cmake for
# the first time.
# This will enable the toolchain file for android and require the NDK at a certain location.
# NOTE: Once configured, -DBUILD_ANDROID=YES can not be changed anymore.
#

if (BUILD_ANDROID AND NOT BUILD_ANDROID_CACHE)
  # Make the flag persistent - changing it is only possible by deleting the generated cmake files of
  # the build.
  set(BUILD_ANDROID_CACHE YES CACHE INTERNAL "Android build is enabled, android toolchain file is being used." FORCE)
endif ()
if (BUILD_ANDROID_CACHE)
  set(BUILD_ANDROID YES)
endif ()

if (BUILD_ANDROID)
  # NOTE: CMAKE_TOOLCHAIN_FILE must be BEFORE project()!
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/android/cmake/android.toolchain.cmake")
  # Variables used by CMAKE_TOOLCHAIN_FILE:
  # NOTE: ANDROID_NDK is preferred over ANDROID_STANDALONE_TOOLCHAIN, if present.
  if(DEFINED ENV{ANDROID_NDK})
    set(ANDROID_NDK $ENV{ANDROID_NDK})
  else()
    # common fallback
    set(ANDROID_NDK "/opt/android-ndk-r10e")
  endif()
  message(STATUS "Using NDK directory: ${ANDROID_NDK}")
  # Force using gcc 4.9
  #  NOTE: available toolchains: contents of ${ANDROID_NDK}/toolchains/
  # TODO: how to support multiple platforms?
  set(ANDROID_TOOLCHAIN_NAME "arm-linux-androideabi-clang3.6")
endif ()


project(openrvclient)
# NOTE: cmake support for Qt5 recommends at least 2.8.11
#       Qt support in previous cmake versions works differently, we do not support that.
cmake_minimum_required(VERSION 2.8.11)

include(${CMAKE_SOURCE_DIR}/version.cmake)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O3")
  set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-DNDEBUG -O3 -g")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-DNDEBUG -O3 -g")


  #
  # Warning flags
  #
  set(c_warnings "")
  set(cxx_warnings "")
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(c_warnings "-W -Wextra -Wall -Wno-long-long -Wcast-align -Wformat-security -Wformat-nonliteral -Wpointer-arith -Wundef -Wchar-subscripts -Wmissing-format-attribute -Wwrite-strings -Wswitch")
    set(cxx_warnings "${c_warnings} -Wnon-virtual-dtor -Woverloaded-virtual")
    #set(cxx_warnings "${c_warnings} -Wno-unused-parameter")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(c_warnings "-W -Wextra -Wall")
    set(cxx_warnings "${c_warnings}")
    if (CLANG_WMOSTLY_EVERYTHING)
      set(c_everything_warnings "-Weverything -Wno-unused-private-field -Wno-unused-macros -Wno-documentation -Wno-c++98-compat-pedantic -Wno-padded -Wno-sign-conversion -Wno-float-equal -Wno-unused-parameter -Wno-extra-semi -Wno-weak-vtables -Wno-conversion -Wno-missing-noreturn -Wno-missing-prototypes -Wno-shadow -Wno-cast-align -Wno-disabled-macro-expansion -Wno-unreachable-code -Wno-covered-switch-default")
      set(c_warnings "${c_warnings} ${c_everything_warnings}")
      set(cxx_warnings "${c_warnings}")
    else()
      set(c_warnings "-W -Wextra -Wall -Wno-long-long -Wcast-align -Wformat-security -Wformat-nonliteral -Wpointer-arith -Wundef -Wchar-subscripts -Wmissing-format-attribute -Wwrite-strings -Wmost")
      set(cxx_warnings "${c_warnings} -Wnon-virtual-dtor -Woverloaded-virtual")
    endif ()
  endif ()

  # NOTE: C90 (not C99) for C code, as MSVC does not support C99, so we should
  #       write code that works without C99.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${c_warnings} -std=c90")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxx_warnings} -std=c++11")
endif ()

# default build type
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)



find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

if(APPLE)
  # set hints to find homebrew installation
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local/opt/openssl)
endif()
find_package(OpenSSL)

# mbedtls
find_path(MBEDTLS_INCLUDE_DIRS
  mbedtls/rsa.h
  PATHS $ENV{HOME}/vcs/extern/mbedtls/include
)
find_library(MBEDTLS_LIBRARY_TLS
  mbedtls
  PATHS $ENV{HOME}/vcs/extern/mbedtls/build/library
)
find_library(MBEDTLS_LIBRARY_CRYPTO
  mbedcrypto
  PATHS $ENV{HOME}/vcs/extern/mbedtls/build/library
)
find_library(MBEDTLS_LIBRARY_X509
  mbedx509
  PATHS $ENV{HOME}/vcs/extern/mbedtls/build/library
)
set(MBEDTLS_LIBRARIES ${MBEDTLS_LIBRARY_TLS} ${MBEDTLS_LIBRARY_X509} ${MBEDTLS_LIBRARY_CRYPTO} )
if (NOT MBEDTLS_INCLUDE_DIRS OR NOT MBEDTLS_LIBRARY_TLS OR NOT MBEDTLS_LIBRARY_CRYPTO OR NOT MBEDTLS_LIBRARY_X509)
  message(WARNING "mbedtls not found, encryption DISABLED")
  set(OPENRV_HAVE_MBEDTLS FALSE)
  set(MBEDTLS_LIBRARY_TLS "")
  set(MBEDTLS_LIBRARY_CRYPTO "")
  set(MBEDTLS_LIBRARY_X509 "")
  set(MBEDTLS_INCLUDE_DIRS "")
  set(MBEDTLS_LIBRARIES "")
else()
  set(OPENRV_HAVE_MBEDTLS TRUE)
endif ()

if (NOT OPENSSL_FOUND)
  message(WARNING "OpenSSL not found, legacy support for anon-TLS security type (used by vino only) is disabled")
  set(OPENRV_HAVE_OPENSSL FALSE)
  set(OPENSSL_INCLUDE_DIR "")
  set(OPENSSL_LIBRARIES "")
else ()
  set(OPENRV_HAVE_OPENSSL TRUE)
endif ()


if (NOT ANDROID)
  set(BUILD_QT_APP TRUE)
  #PATHS ${HOME}/Qt/5.5/clang_64/bin
  if (BUILD_QT_APP)
    find_package(OpenGL)
    set(required_qt_modules Qt5Core Qt5OpenGL Qt5Network Qt5Gui Qt5Widgets)
    if (NOT OPENGL_FOUND)
      message(WARNING "OpenGL headers/library not found. Not building the Qt app.")
      set(BUILD_QT_APP FALSE)
    endif ()
  endif ()
  if (BUILD_QT_APP)
    foreach (q ${required_qt_modules})
      find_package(${q}
        PATHS $ENV{HOME}/Qt/5.5/clang_64
              $ENV{HOME}/dev/Qt/5.3/clang_64
      )
      if (NOT ${q}_FOUND)
        if (BUILD_QT_APP)
          message(WARNING "Qt module ${q} not found. Not building the Qt app.")
        endif ()
        set(BUILD_QT_APP FALSE)
      elseif (${q}_VERSION VERSION_LESS "5.4.0")
        # QOpenGLWidget exists since Qt 5.4, we do not provide non-GL variants or QGLWidget based
        # variants atm.
        if (BUILD_QT_APP)
          message(WARNING "Require at least Qt 5.4, found: ${${q}_VERSION}. Not building the Qt app.")
        endif ()
        set(BUILD_QT_APP FALSE)
      endif ()
    endforeach ()
  endif ()
endif ()

include_directories(${CMAKE_BINARY_DIR})

set(libopenrv_thirdparty_SRCS
  ${CMAKE_SOURCE_DIR}/thirdparty/libtomcrypt/des.c
)

set(libopenrv_mbedtls_SRCS)
if (OPENRV_HAVE_MBEDTLS)
  set(libopenrv_mbedtls_SRCS
    libopenrv/mbedtlscontext.cpp
  )
endif ()
if (OPENRV_HAVE_OPENSSL)
  set(libopenrv_openssl_SRCS
    libopenrv/opensslcontext.cpp
  )
endif ()
set(libopenrv_public_HDRS
  libopenrv/public/libopenrv/libopenrv.h
  libopenrv/public/libopenrv/orv_error.h
  libopenrv/public/libopenrv/orv_errorcodes.h
  libopenrv/public/libopenrv/orv_latencytesterclient.h
  libopenrv/public/libopenrv/orv_logging.h
  libopenrv/public/libopenrv/orv_qtkey_to_xkeysym.h
)
set(libopenrv_SRCS
  libopenrv/orv_error.cpp
  libopenrv/orv_logging.cpp
  libopenrv/libopenrv.cpp
  libopenrv/utils.cpp
  libopenrv/orvvncclientshareddata.cpp
  libopenrv/rfb3xhandshake.cpp
  libopenrv/orvvncclient.cpp
  libopenrv/socket.cpp
  libopenrv/threadnotifier.cpp
  libopenrv/securitytypehandler.cpp
  libopenrv/eventqueue.cpp
  libopenrv/vncdes.cpp
  libopenrv/messageparser.cpp
  libopenrv/rectdataparser.cpp
  libopenrv/key_android.cpp
  libopenrv/keys.cpp
  libopenrv/orv_latencytesterclient.cpp
  ${libopenrv_mbedtls_SRCS}
  ${libopenrv_openssl_SRCS}
  ${libopenrv_thirdparty_SRCS}
  ${libopenrv_public_HDRS}
)
add_library(libopenrv STATIC ${libopenrv_SRCS})
target_include_directories(libopenrv SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/libtomcrypt ${MBEDTLS_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})
target_include_directories(libopenrv PUBLIC ${CMAKE_SOURCE_DIR}/libopenrv/public ${CMAKE_BINARY_DIR}/libopenrv/public)
configure_file(${CMAKE_SOURCE_DIR}/libopenrv/public/libopenrv/orv_version.h.cmake ${CMAKE_BINARY_DIR}/libopenrv/public/libopenrv/orv_version.h)
configure_file(${CMAKE_SOURCE_DIR}/libopenrv/public/libopenrv/orv_config.h.cmake ${CMAKE_BINARY_DIR}/libopenrv/public/libopenrv/orv_config.h)
target_link_libraries(libopenrv ${MBEDTLS_LIBRARIES} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES})

if (NOT ANDROID)
  add_executable(openrv_cmdline cmdline/main.cpp)
  target_include_directories(openrv_cmdline PUBLIC ${CMAKE_SOURCE_DIR}/libopenrv/public ${CMAKE_BINARY_DIR}/libopenrv/public)
  target_link_libraries(openrv_cmdline
    libopenrv
    ${CMAKE_THREAD_LIBS_INIT}
  )


  if (BUILD_QT_APP)
    set(openrvclient_qt_srcs
      qt/main.cpp
      qt/orv_context_qt.cpp
      qt/mainwindow.cpp
      qt/topwidget.cpp
      qt/serverlistmodel.cpp
      qt/serverlistwidget.cpp
      qt/newserverwidget.cpp
      qt/connectstatewidget.cpp
      qt/orvwidget.cpp
      qt/framebufferwidget.cpp
      qt/connectioninfowidget.cpp
    )
    set(openrvclient_qt_hdrs
      qt/orv_context_qt.h
      qt/topwidget.h
      qt/serverlistmodel.h
      qt/serverlistwidget.h
      qt/mainwindow.h
      qt/newserverwidget.h
      qt/connectstatewidget.h
      qt/orvwidget.h
      qt/framebufferwidget.h
      qt/connectioninfowidget.h
    )
    qt5_wrap_cpp(
      openrvclient_qt_srcs
      ${openrvclient_qt_hdrs}
    )
    #qt5_wrap_ui(
    #  openrvclient_qt_srcs
    #  qt/window.ui
    #)
    add_executable(openrvclient ${openrvclient_qt_srcs} ${openrvclient_qt_hdrs})
    target_include_directories(openrvclient
      PUBLIC
      ${CMAKE_SOURCE_DIR}/libopenrv/public
      ${CMAKE_BINARY_DIR}/libopenrv/public
    )
    target_include_directories(openrvclient
      SYSTEM
      PRIVATE
      ${Qt5Core_INCLUDE_DIRS}
      ${Qt5OpenGL_INCLUDE_DIRS}
      ${Qt5Network_INCLUDE_DIRS}
      ${Qt5Gui_INCLUDE_DIRS}
      ${Qt5Widgets_INCLUDE_DIRS}
      ${Qt5Test_INCLUDE_DIRS}
      ${OPENGL_INCLUDE_DIR}
    )
    set_target_properties(openrvclient PROPERTIES COMPILE_DEFINITIONS
      "${Qt5Core_COMPILE_DEFINITIONS};${Qt5OpenGL_COMPILE_DEFINITIONS};${Qt5Network_COMPILE_DEFINITIONS};${Qt5Gui_COMPILE_DEFINITIONS};${Qt5Widgets_COMPILE_DEFINITIONS}"
    )
    if (APPLE)
      set_target_properties(openrvclient
        PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME          "OpenRVClient"
        #MACOSX_BUNDLE_INFO_STRING          ""
        # MACOSX_BUNDLE_ICON_FILE            "" # TODO
        MACOSX_BUNDLE_LONG_VERSION_STRING  "${LIBOPENRV_VERSION_STRING}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${LIBOPENRV_VERSION_MAJOR}.${LIBOPENRV_VERSION_MINOR}.${LIBOPENRV_VERSION_PATCH}"
        #MACOSX_BUNDLE_BUNDLE_VERSION       ""
        MACOSX_BUNDLE_COPYRIGHT            "${LIBOPENRV_COPYRIGHT_STRING}"
      )
    endif ()
    target_link_libraries(openrvclient
      libopenrv
      ${CMAKE_THREAD_LIBS_INIT}
      Qt5::Core
      Qt5::Network
      Qt5::Gui
      Qt5::Widgets
      ${OPENGL_gl_LIBRARY}
    )
  endif ()
endif (NOT ANDROID)

# TODO: make install target

# vim: et sw=2 sts=2 ts=2
