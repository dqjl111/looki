cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
    add_library(lksvg SHARED)
else()
    add_library(lksvg
        lsvgrenderer.h lsvgrenderer.cpp)
endif()

target_compile_features(lksvg PUBLIC cxx_std_23)
target_compile_definitions(lksvg PRIVATE LKBUILD_SVG)

set(looki_SVGDIRS
    /lkgraph/svg
)

foreach(subdir ${looki_SVGDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKSVG_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKSVG_HEADERS ${TMP_HEADERS})
endforeach()


target_include_directories(lksvg PRIVATE ${looki_SOURCE_DIR})
target_sources(lksvg PRIVATE ${LKSVG_SOURCES} ${LKSVG_HEADERS})

target_link_libraries(lksvg lkcore lkgui ${looki_DEPEND_LIBRARIES})


