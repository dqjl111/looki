cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
    add_library(lkcore SHARED)
else()
    add_library(lkcore)
endif()

target_compile_definitions(lkcore PRIVATE LKBUILD_CORE)
target_compile_features(lkcore PUBLIC cxx_std_23)

set(looki_COREDIRS
    /lkcore/3rd
    /lkcore
    /lkcore/io
    /lkcore/text
    /lkcore/time
    /lkcore/plugin
)

if(WIN32)
    list(APPEND looki_COREDIRS
        /lkcore/osspec/win
    )
elseif(UNIX)
    list(APPEND looki_COREDIRS
        /lkcore/osspec/unix
    )
endif()

foreach(subdir ${looki_COREDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKCORE_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKCORE_HEADERS ${TMP_HEADERS})
endforeach()

target_include_directories(lkcore PRIVATE ${looki_SOURCE_DIR})
target_sources(lkcore PRIVATE ${LKCORE_SOURCES} ${LKCORE_HEADERS})
target_link_libraries(lkcore ${looki_DEPEND_LIBRARIES})
