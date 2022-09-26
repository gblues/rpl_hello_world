cmake_minimum_required(VERSION 3.2)
project(libhello C)

add_executable(libhello lib_hello.c)
wut_add_exports(libhello exports.def)
wut_create_rpl(libhello)