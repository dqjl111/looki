cmake_minimum_required(VERSION 3.5)

add_library(lkentrypoint)
target_compile_features(lkentrypoint PUBLIC cxx_std_23)

target_sources(lkentrypoint PRIVATE ${looki_SOURCE_DIR}/lkentrypoint/lmain.cpp)
target_link_libraries(lkentrypoint ${looki_DEPEND_LIBRARIES})

