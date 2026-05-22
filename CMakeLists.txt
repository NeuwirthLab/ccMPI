cmake_minimum_required(VERSION 3.20)
project(cache_bench
  VERSION 1.0
  LANGUAGES C CXX ASM
)

# ---------------------------------------------------------------
# Module path
# ---------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------
# Default build type
# ---------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting default build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
endif()

# ---------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11)

find_package(MPI REQUIRED)

# ---------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------
add_subdirectory(libs)

# ---------------------------------------------------------------
# Helper
# ---------------------------------------------------------------
function(set_properties TARGET)
	target_link_libraries(${TARGET}
	  PUBLIC
	    CacheFlush
	    PAPIWrapper
	    MPI::MPI_CXX
	    CLI11::CLI11
	)
	target_include_directories(${TARGET}
	  PUBLIC
	    ${CMAKE_SOURCE_DIR}/include
	)
	target_compile_features(${TARGET}
	  PUBLIC
	    cxx_std_17
	)
endfunction()

# ---------------------------------------------------------------
# Main executables
# ---------------------------------------------------------------
add_executable(send_recv.bin
  src/send_recv.cpp
)
set_properties(send_recv.bin)
add_executable(send_recv_blocking.bin
  src/send_recv.cpp
)
set_properties(send_recv_blocking.bin)
target_compile_definitions(send_recv_blocking.bin PRIVATE USE_BLOCKING_PT2PT)

#add_executable(allreduce.bin src/allreduce.cpp)
#set_properties(allreduce.bin)

add_executable(send_recv_bounce_buffer.bin src/send_recv_bbuffer.cpp)
set_properties(send_recv_bounce_buffer.bin)
target_link_libraries(send_recv_bounce_buffer.bin PRIVATE dl)

add_executable(memcpy.bin src/memcpy.cpp)
target_link_libraries(memcpy.bin PUBLIC CacheFlush PAPIWrapper CLI11::CLI11)
target_include_directories(memcpy.bin PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_features(memcpy.bin PUBLIC cxx_std_17)


#add_executable(memcpy.bin src/memcpy.cpp src/avx_memcpy.S src/ucx_memcpy.S)
#target_link_libraries(memcpy.bin PUBLIC CacheFlush PAPIWrapper CLI11::CLI11)
#target_include_directories(memcpy.bin PUBLIC ${CMAKE_SOURCE_DIR}/include)
#target_compile_features(memcpy.bin PUBLIC cxx_std_17)
#target_compile_options(memcpy.bin PRIVATE -mavx2)

# ---------------------------------------------------------------
# Summary
# ---------------------------------------------------------------
message(STATUS "")
message(STATUS "================= Build Configuration =================")
message(STATUS "Project:          ${PROJECT_NAME}")
message(STATUS "Version:          ${PROJECT_VERSION}")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler:       ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler:     ${CMAKE_CXX_COMPILER}")
message(STATUS "MPI found:        ${MPI_FOUND}")
message(STATUS "PAPI_LIBRARY:	  ${PAPI_LIBRARY}")
message(STATUS "=======================================================")
message(STATUS "")
