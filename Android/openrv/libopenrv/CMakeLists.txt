cmake_minimum_required(VERSION 3.4.1)

# Path to the main openrv directory (containing CMakeLists.txt)
add_subdirectory(../../../ ${CMAKE_CURRENT_BINARY_DIR}/openrv)

find_library(log-lib log)

set(openrv_android_SRCS
    src/main/cpp/openrv_jni.cpp

    # include the openrv_object object library (cmake convenience library providing the object
    # files) in this library (defined by the main CMakeLists.txt).
    $<TARGET_OBJECTS:openrv_object>
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${libopenrv_object_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${libopenrv_object_CXX_FLAGS}")
add_library(openrv_android SHARED ${openrv_android_SRCS})

target_link_libraries(
    openrv_android
    ${log-lib}

    # NOTE: The libraries that the openrv_object library depends on.
    #       Apparently cmake currently provides to way to automatically pulling these in when adding
    #       the object library to a target (target_link_libraries cannot be used on object libraries
    #       currently). Therefore we track them in a separate variable in the main CMakeLists.txt
    #       file and need to link to them manually here.
    ${libopenrv_object_LIBRARIES}
)
# NOTE: The include directories for OpenRV.
#       Similar problem to target_link_libraries() above, apparently cmake object libraries don't
#       carry the required include directories over, so we use a a dedicated variable from the main
#       CMakeLists.txt file here.
target_include_directories(openrv_android PUBLIC ${libopenrv_object_INCLUDE_DIRS})

