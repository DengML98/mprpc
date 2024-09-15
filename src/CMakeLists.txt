file(GLOB_RECURSE src 
    ./*.cc 
    ../thirdparty/simpleini/*.cc 
)
add_library(mprpc SHARED ${src}) 
target_link_libraries(mprpc muduo_net muduo_base pthread protobuf zookeeper_mt) #多线程的zookeeper库