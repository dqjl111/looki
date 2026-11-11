cmake_minimum_required(VERSION 3.5)

if(looki_BUILD_SHARED)
    add_library(lkgui SHARED)
else()
    add_library(lkgui)
endif()

target_compile_definitions(lkgui PRIVATE LKBUILD_GUI)
target_compile_features(lkgui PUBLIC cxx_std_23)

set(looki_GUIDIRS
    /lkgui/3rd
    /lkgui
    /lkgui/image
    /lkgui/math
    /lkgui/paint
    /lkgui/text

)

if(WIN32)
    list(APPEND looki_GUIDIRS
        /lkgui/osspec/win
        /lkgui/osspec/win/direct2d
    )
    if(looki_BUILD_3D)
        list(APPEND looki_GUIDIRS
            /lkgui/rhi
            /lkgui/rhi/d3d
        )
    endif()
elseif(UNIX)
    list(APPEND looki_GUIDIRS
        /lkgui/osspec/unix
    )
    if(looki_BUILD_3D)
        list(APPEND looki_GUIDIRS
            /lkgui/rhi
            /lkgui/rhi/vulkan
        )
    endif()
endif()

foreach(subdir ${looki_GUIDIRS})
    aux_source_directory(${looki_SOURCE_DIR}${subdir} LKGUI_SOURCES)
    file(GLOB TMP_HEADERS ${looki_SOURCE_DIR}${subdir}/*.h ${looki_SOURCE_DIR}${subdir}/*.hpp)
    list(APPEND LKGUI_HEADERS ${TMP_HEADERS})
endforeach()

target_include_directories(lkgui PRIVATE ${looki_SOURCE_DIR})
target_sources(lkgui PRIVATE ${LKGUI_SOURCES} ${LKGUI_HEADERS})
target_link_libraries(lkgui lkcore ${looki_DEPEND_LIBRARIES})

