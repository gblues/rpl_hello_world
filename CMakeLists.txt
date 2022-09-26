cmake_minimum_required(VERSION 3.2)
project(demo C)

add_executable(hello_world hello_world_demo.c)
wut_create_rpx(hello_world)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hello_world_demo.rpx"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lib_hello.rpl"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")
