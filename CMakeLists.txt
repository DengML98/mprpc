cmake_minimum_required(VERSION 3.0)
if (NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Release) 
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_STANDARD_REQUIRED ON) #必须支持指定的c++标准
set(CMAKE_CXX_EXTENSIONS OFF)  #关闭gcc扩展

project(mprpc)
set(CMAKE_CXX_STANDARD 20)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output) #程序生成位置, PROJECT_SOURCE_DIR是当前项目的顶级路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output)  #库生成位置

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/thirdparty  #muduo库要用boost库的头文件
    ${PROJECT_SOURCE_DIR}/thirdparty/muduo/include
    ${PROJECT_SOURCE_DIR}/thirdparty/protobuf-3.21.12/include
    ${PROJECT_SOURCE_DIR}/thirdparty/simpleini
    ${PROJECT_SOURCE_DIR}/thirdparty/zookeeper-3.4.9/include
    ${PROJECT_SOURCE_DIR}/example
    ${PROJECT_SOURCE_DIR}/thirdparty/mysqlclient
) 
link_directories(
    ${PROJECT_SOURCE_DIR}/output #mprpc.so
    ${PROJECT_SOURCE_DIR}/thirdparty/muduo/lib
    ${PROJECT_SOURCE_DIR}/thirdparty/protobuf-3.21.12/lib
    ${PROJECT_SOURCE_DIR}/thirdparty/zookeeper-3.4.9/lib
) 

add_subdirectory(src)
add_subdirectory(example/server)
add_subdirectory(example/client)