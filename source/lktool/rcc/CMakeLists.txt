cmake_minimum_required(VERSION 3.5)

file(GLOB RCC_TOOL_FILES ${looki_SOURCE_DIR}/lktool/rcc/*.h ${looki_SOURCE_DIR}/lktool/rcc/*.cpp)
add_executable(lrcc ${RCC_TOOL_FILES})
target_compile_features(lrcc PUBLIC cxx_std_23)
target_include_directories(lrcc PRIVATE ${looki_SOURCE_DIR})

target_link_libraries(lrcc lkentrypoint lkcore  ${looki_DEPEND_LIBRARIES})
