#
# Copyright (C) 2018 Monument-Software GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
project(openrvclient)
cmake_minimum_required(VERSION 3.0.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)

set(ORV_BUILD_STATIC_LIB_DEFAULT ON)
set(ORV_BUILD_SHARED_LIB_DEFAULT ON)
if (ANDROID)
  set(ORV_BUILD_STATIC_LIB_DEFAULT OFF)
  set(ORV_BUILD_SHARED_LIB_DEFAULT ON)
endif ()

option(ORV_BUILD_STATIC_LIB
  "Build a static library."
  ${ORV_BUILD_STATIC_LIB_DEFAULT}
)
option(ORV_BUILD_SHARED_LIB
  "Build a shared library."
  ${ORV_BUILD_SHARED_LIB_DEFAULT}
)
if (NOT ANDROID)
  option(ORV_BUILD_CMDLINE
    "Build the command line test tool for OpenRV, if supported by the target platform."
    ON
  )
  option(ORV_BUILD_QT_CLIENT
    "Build the Qt test client, if the Qt libraries are found. Note that building the client will be
    disabled with a warning, if the libraries are not found."
    ON
  )
else ()
  set(ORV_BUILD_CMDLINE OFF)
  set(ORV_BUILD_QT_CLIENT OFF)
endif ()

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
)
find_library(MBEDTLS_LIBRARY_TLS
  mbedtls
)
find_library(MBEDTLS_LIBRARY_CRYPTO
  mbedcrypto
)
find_library(MBEDTLS_LIBRARY_X509
  mbedx509
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


if (ORV_BUILD_QT_CLIENT)
  find_package(OpenGL)
  set(required_qt_modules Qt5Core Qt5OpenGL Qt5Network Qt5Gui Qt5Widgets)
  if (NOT OPENGL_FOUND)
    message(WARNING "OpenGL headers/library not found. Not building the Qt app.")
    set(BUILD_QT_APP FALSE)
  endif ()
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

# NOTE: We have some generated headers that should be included using #include "" notation
#       (instead of <>) as well, include_directories() does not support that. so we use
#       -iquote manually.
set(libopenrv_object_C_FLAGS "-iquote ${CMAKE_CURRENT_BINARY_DIR}/libopenrv/public/libopenrv")
set(libopenrv_object_CXX_FLAGS "-iquote ${CMAKE_CURRENT_BINARY_DIR}/libopenrv/public/libopenrv")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${libopenrv_object_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${libopenrv_object_CXX_FLAGS}")

set(libopenrv_thirdparty_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libtomcrypt/des.c
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


# add an object library, a "virtual" library that is cmake internal and merely groups the object
# files into a single target.
# it is meant to be used by other cmake targets only and produces no library output by itself.
add_library(openrv_object OBJECT ${libopenrv_SRCS})
set_target_properties(openrv_object PROPERTIES POSITION_INDEPENDENT_CODE True)
target_include_directories(openrv_object SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libtomcrypt ${MBEDTLS_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})
set(libopenrv_object_public_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/libopenrv/public)
target_include_directories(openrv_object PUBLIC ${libopenrv_object_public_INCLUDE_DIRS})

# cmake object libraries are currently not allowed to link to anything (apparently this is a cmake
# todo). so we track the dependencies in a separate variable and the individual targets need to link
# to them explicitly.
set(libopenrv_object_LIBRARIES ${MBEDTLS_LIBRARIES} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES})

# provide libopenrv_object_LIBRARIES and libopenrv_object_INCLUDE_DIRS to the parent scope,
# in case this CMakeLists.txt file is not the toplevel file (used by the android library project)
get_directory_property(cmake_parent_directory PARENT_DIRECTORY)
if (cmake_parent_directory)
  set(libopenrv_object_LIBRARIES ${libopenrv_object_LIBRARIES} PARENT_SCOPE)
  set(libopenrv_object_INCLUDE_DIRS ${libopenrv_object_public_INCLUDE_DIRS} PARENT_SCOPE)
  set(libopenrv_object_C_FLAGS "${libopenrv_object_C_FLAGS}" PARENT_SCOPE)
  set(libopenrv_object_CXX_FLAGS "${libopenrv_object_CXX_FLAGS}" PARENT_SCOPE)
endif ()

if (ORV_BUILD_STATIC_LIB)
  add_library(openrv STATIC $<TARGET_OBJECTS:openrv_object>)
endif ()
if (ORV_BUILD_SHARED_LIB)
  add_library(openrv_shared SHARED $<TARGET_OBJECTS:openrv_object>)
  target_link_libraries(openrv_shared ${libopenrv_object_LIBRARIES})
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libopenrv/public/libopenrv/orv_version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libopenrv/public/libopenrv/orv_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libopenrv/public/libopenrv/orv_config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libopenrv/public/libopenrv/orv_config.h)

if (ORV_BUILD_CMDLINE)
  add_executable(openrv_cmdline cmdline/main.cpp $<TARGET_OBJECTS:openrv_object>)
  target_include_directories(openrv_cmdline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libopenrv/public ${CMAKE_CURRENT_BINARY_DIR}/libopenrv/public)
  target_link_libraries(openrv_cmdline
    ${libopenrv_object_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )
endif ()

if (ORV_BUILD_QT_CLIENT)
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
  add_executable(openrvclient ${openrvclient_qt_srcs} ${openrvclient_qt_hdrs} $<TARGET_OBJECTS:openrv_object>)
  target_link_libraries(openrvclient
    ${libopenrv_object_LIBRARIES}
  )
  target_include_directories(openrvclient
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libopenrv/public
    ${CMAKE_CURRENT_BINARY_DIR}/libopenrv/public
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
    ${CMAKE_THREAD_LIBS_INIT}
    Qt5::Core
    Qt5::Network
    Qt5::Gui
    Qt5::Widgets
    ${OPENGL_gl_LIBRARY}
  )
endif ()

# TODO: make install target

# vim: et sw=2 sts=2 ts=2
