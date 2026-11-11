cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
	add_library(lkwidgets SHARED)
else()
	add_library(lkwidgets)
endif()

target_compile_definitions(lkwidgets PRIVATE LKBUILD_WIDGETS)
target_compile_features(lkwidgets PUBLIC cxx_std_23)

set(looki_WIDGETSDIRS
    /lkwidgets/core
    /lkwidgets/util
    /lkwidgets/draw
    /lkwidgets/widget
    /lkwidgets/dialog
)

foreach(subdir ${looki_WIDGETSDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKWIDGETS_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKWIDGETS_HEADERS ${TMP_HEADERS})
endforeach()

target_include_directories(lkwidgets PRIVATE ${looki_SOURCE_DIR})
target_sources(lkwidgets PRIVATE ${LKWIDGETS_SOURCES} ${LKWIDGETS_HEADERS})
target_link_libraries(lkwidgets lkgui lkcore ${looki_DEPEND_LIBRARIES})
