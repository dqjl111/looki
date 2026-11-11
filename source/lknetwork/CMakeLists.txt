cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
    add_library(lknetwork SHARED)
else()
    add_library(lknetwork)
endif()

target_compile_definitions(lknetwork PRIVATE LKBUILD_NETWORK)
target_compile_features(lknetwork PUBLIC cxx_std_23)


set(looki_NETWORKDIRS
    /lknetwork
)

if(looki_USE_SSL)
    list(APPEND looki_NETWORKDIRS
        /lknetwork/ssl
    )
endif()

foreach(subdir ${looki_NETWORKDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKNETWORK_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKNETWORK_HEADERS ${TMP_HEADERS})
endforeach()


target_include_directories(lknetwork PRIVATE ${looki_SOURCE_DIR})
target_sources(lknetwork PRIVATE ${LKNETWORK_SOURCES} ${LKNETWORK_HEADERS})

if(WIN32)
target_link_libraries(lknetwork lkcore Ws2_32 Mswsock AdvApi32 ${looki_DEPEND_LIBRARIES} ${looki_SSL_LIBRARIES})
else()
target_link_libraries(lknetwork lkcore ${looki_DEPEND_LIBRARIES} ${looki_SSL_LIBRARIES})
endif()

