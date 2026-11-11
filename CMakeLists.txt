cmake_minimum_required(VERSION 3.5)

set(LOOKI_VERSION 0.0.1)
project(looki VERSION ${LOOKI_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_definitions(_GLIBCXX_USE_WCHAR_T)

option(looki_CMAKE_ENABLE_TEST "enable test" OFF)
option(looki_BUILD_NAMESPACE "build lk namespace" OFF)
option(looki_BUILD_SHARED "build lk shared" OFF)
option(looki_BUILD_3D "build lk 3D" OFF)

option(looki_USE_SSL "build with ssl" OFF)
option(looki_USE_SQLITE "build with sqlite" OFF)


set(looki_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source)
if(WIN32)
set(VCPKG_ROOT "D:/vcpkg/vcpkg-2026.07.29"  CACHE STRING "vcpkg root")
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
# set(VCPKG_INCLUDE_DIR ${VCPKG_ROOT}/installed/x64-windows/include)
# include_directories(${VCPKG_INCLUDE_DIR})
endif()


if(looki_BUILD_NAMESPACE)
  set(LOOKI_NAMESPACE 1)
else()
  set(LOOKI_NAMESPACE 0)
endIf()

if(looki_BUILD_SHARED)
  set(LOOKI_BUILD_SHARED 1)
else()
  set(LOOKI_BUILD_SHARED 0)
endIf()

set(looki_DEPEND_LIBRARIES "")
set(looki_SSL_LIBRARIES "")

if(looki_USE_SSL)
  find_package(OpenSSL REQUIRED)
  list(APPEND looki_SSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
endIf()

if(looki_USE_SQLITE)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
endif()

if(CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
  set(LOOKI_LITTLE_ENDIAN 1)
else()
  set(LOOKI_LITTLE_ENDIAN 0)
endif()


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lookiconfig.h.in ${looki_SOURCE_DIR}/lkcore/lookiconfig.h)

# COMMON LIBRARIES
if (UNIX)
    list(APPEND looki_DEPEND_LIBRARIES pthread)
endif()

set(WINENTRY "")
if (MINGW)
  #link_directories(${looki_SOURCE_DIR}/lk3rd/libs-mingw/x86/)
  set(WINENTRY WIN32)
  list(APPEND looki_DEPEND_LIBRARIES pthread)
endif()


if(WIN32)
    #link_directories(${looki_SOURCE_DIR}/lk3rd/libs-msvc/x86/)
  set(WINENTRY WIN32)
  list(APPEND looki_DEPEND_LIBRARIES imm32 dwmapi d3d11 d3dcompiler dxgi d2d1 dwrite)
endif()


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(${looki_SOURCE_DIR})

set(LRCC_GENER ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lrcc${CMAKE_EXECUTABLE_SUFFIX})
include(CMake/LRcc.cmake)

if(looki_CMAKE_ENABLE_TEST AND (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR))
  enable_testing()
  if(NOT MSVC)
      add_compile_options(-Wall -Wextra)
  endif()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()

# INSTALL DIRECTIVES
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/ DESTINATION test)
install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION lib PATTERN "*.exp" EXCLUDE)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/ DESTINATION bin PATTERN "*.ilk" EXCLUDE PATTERN "*.pdb" EXCLUDE PATTERN "*.db" EXCLUDE)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/CMake/ DESTINATION CMake)
