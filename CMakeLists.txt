cmake_minimum_required(VERSION 3.8)
project(raygl)

set(CMAKE_CXX_STANDARD 17)

# this function preppends a path to all files in a list
FUNCTION(PREPEND var prefix)
SET(listVar "")
   FOREACH(f ${ARGN})
   LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

add_subdirectory(libs/glfw)
add_subdirectory(libs/glad)
add_subdirectory(libs/glm)
add_subdirectory(libs/stb)
add_subdirectory(libs/tl)
add_subdirectory(libs/tg)

set(SOURCES
    main.cpp
    utils.hpp utils.cpp
)
PREPEND(SOURCES "src/" ${SOURCES})

add_executable(raygl ${SOURCES})

target_link_libraries(raygl
    glfw
    glad
    glm
    stb
    tl
    tg
)