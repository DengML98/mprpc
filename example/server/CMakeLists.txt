file(GLOB_RECURSE SRC 
    ./*.cpp
   ../user.pb.cc
) #rpcprovider.h是库的头文件，不用包含源文件
add_executable(server ${SRC})
target_link_libraries(server mprpc mysqlclient)