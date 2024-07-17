cmake_minimum_required(VERSION 3.8)

project(nes)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

add_subdirectory(lib/sdl2 EXCLUDE_FROM_ALL)
add_subdirectory(lib/nativefiledialog-extended)

add_executable(nes
	src/main.cpp
	src/nes.cpp
	src/mapper.cpp
	src/cpu.cpp
	src/ppu.cpp
	src/apu.cpp
)

set_property(TARGET nes PROPERTY CXX_STANDARD 20)

if(TARGET SDL2::SDL2main)
    target_link_libraries(nes PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(nes
	PRIVATE SDL2::SDL2-static
	nfd
)
