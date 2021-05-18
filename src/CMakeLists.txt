aux_source_directory(. DIR_LIB_SRCS)
include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    )
add_library (kernel shell.cpp
kernel.cpp editor.cpp 
"log.cpp" filesystem.cpp
memory.cpp process.cpp device.cpp)
add_library (graphics gui.cpp)
target_link_libraries(
    graphics
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLAD_LIBRARIES}
    ${IMGUI_LIBRARIES}
)
if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 /Zc:__cplusplus /D_HAS_STD_BYTE=0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()
