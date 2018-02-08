cmake_minimum_required (VERSION 2.8.11)
project (Donburi)
set(CMAKE_CXX_STANDARD 11) 

set(LIBUNWIND_ROOT /home/don/install/libunwind)

add_library (donburi SHARED MemFunctions.cpp)
target_include_directories (donburi PUBLIC ${LIBUNWIND_ROOT}/include)
target_link_libraries(donburi  ${LIBUNWIND_ROOT}/lib/libunwind.so ${LIBUNWIND_ROOT}/lib/libunwind-x86_64.so dl)

add_executable(test_donburi test.cpp)

add_subdirectory(tools)
