cmake_minimum_required(VERSION 3.5)


add_executable(lktestsignal ${CMAKE_CURRENT_SOURCE_DIR}/testsignal.cpp)
target_include_directories(lktestsignal PRIVATE ${looki_SOURCE_DIR})
target_link_libraries(lktestsignal lkentrypoint lkcore)


list(APPEND TESTWIN_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/edeviceuser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/edeviceuser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testwin.cpp
)

add_executable(lktestwin ${WINENTRY} ${TESTWIN_FILES})
target_include_directories(lktestwin PRIVATE ${looki_SOURCE_DIR})
target_link_libraries(lktestwin lkentrypoint lkcore lkgui lkwidgets lkaudio lknetwork lksql lksvg)
lrcc_add_resources(lktestwin ${CMAKE_CURRENT_SOURCE_DIR}/res.lrc)
