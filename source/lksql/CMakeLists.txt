cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
    add_library(lksql SHARED)
else()
    add_library(lksql)
endif()

target_compile_definitions(lksql PRIVATE LKBUILD_SQL)
target_compile_features(lksql PUBLIC cxx_std_23)

set(looki_SQLDIRS
    /lksql
)

foreach(subdir ${looki_SQLDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKSQL_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKSQL_HEADERS ${TMP_HEADERS})
endforeach()


target_include_directories(lksql PRIVATE ${looki_SOURCE_DIR})
target_sources(lksql PRIVATE ${LKSQL_SOURCES} ${LKSQL_HEADERS})

if(WIN32)
target_link_libraries(lksql lkcore ${looki_DEPEND_LIBRARIES})
else()
target_link_libraries(lksql lkcore ${looki_DEPEND_LIBRARIES})
endif()

