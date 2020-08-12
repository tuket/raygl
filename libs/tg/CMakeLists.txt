project(tg)

add_library(tg
	img.hpp img.cpp
	shader_utils.hpp shader_utils.cpp
)

target_include_directories(tg PUBLIC  ${PROJECT_SOURCE_DIR}/..)

set(COMMON_LIBS
	glm
	stb
	tl
)

if(EMSCRIPTEN)
	target_link_libraries(tg
		${COMMON_LIBS}
	)
else()
	target_link_libraries(tg
		${COMMON_LIBS}
		glad
	)
endif()
