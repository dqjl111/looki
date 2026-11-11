cmake_minimum_required(VERSION 3.5)

add_library(lksqlite SHARED)

target_compile_features(lksqlite PUBLIC cxx_std_23)

set(looki_SQLITEDIRS
    /lkplugins/sqldrivers/sqlite
)

foreach(subdir ${looki_SQLITEDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKSQLITE_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKSQLITE_HEADERS ${TMP_HEADERS})
endforeach()


target_include_directories(lksqlite PRIVATE ${looki_SOURCE_DIR})
target_sources(lksqlite PRIVATE ${LKSQLITE_SOURCES} ${LKSQLITE_HEADERS})

if(WIN32)
target_link_libraries(lksqlite lkcore lksql unofficial::sqlite3::sqlite3 ${looki_DEPEND_LIBRARIES})
else()
target_link_libraries(lksqlite lkcore lksql unofficial::sqlite3::sqlite3 ${looki_DEPEND_LIBRARIES})
endif()

set_target_properties(lksqlite PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/sqldrivers/)

