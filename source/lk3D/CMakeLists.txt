cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
    add_library(lk3D SHARED)
else()
    add_library(lk3D)
endif()

target_compile_definitions(lk3D PRIVATE LKBUILD_3D)
target_compile_features(lk3D PUBLIC cxx_std_23)

set(looki_3DDIRS
/lk3D
/lk3D/core
/lk3D/render
)

foreach(subdir ${looki_3DDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LK3D_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LK3D_HEADERS ${TMP_HEADERS})
endforeach()

target_include_directories(lk3D PRIVATE ${looki_SOURCE_DIR})
target_sources(lk3D PRIVATE ${LK3D_SOURCES} ${LK3D_HEADERS})
target_link_libraries(lk3D lkgui lkcore ${looki_DEPEND_LIBRARIES})
