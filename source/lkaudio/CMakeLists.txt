cmake_minimum_required(VERSION 3.5)

add_library(miniaudio)
set_target_properties(miniaudio PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(miniaudio PUBLIC cxx_std_23)

set(MINIAUDIO_DIRS
    /lkaudio/3rd
)

foreach(subdir ${MINIAUDIO_DIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} MINIAUDIO_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND MINIAUDIO_HEADERS ${TMP_HEADERS})
endforeach()

target_sources(miniaudio PRIVATE ${MINIAUDIO_SOURCES} ${MINIAUDIO_HEADERS})
list(APPEND INSTALL_BINS miniaudio)
list(APPEND INSTALL_LIBS miniaudio)


if(looki_BUILD_SHARED)
    add_library(lkaudio SHARED)
else()
    add_library(lkaudio)
endif()

target_compile_definitions(lkaudio PRIVATE LKBUILD_AUDIO)
target_compile_features(lkaudio PUBLIC cxx_std_23)


set(looki_AUDIODIRS
    /lkaudio
)

foreach(subdir ${looki_AUDIODIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKAUDIO_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKAUDIO_HEADERS ${TMP_HEADERS})
endforeach()

target_include_directories(lkaudio PRIVATE ${looki_SOURCE_DIR})
target_sources(lkaudio PRIVATE ${LKAUDIO_SOURCES} ${LKAUDIO_HEADERS})
target_link_libraries(lkaudio lkcore miniaudio ${looki_DEPEND_LIBRARIES})
