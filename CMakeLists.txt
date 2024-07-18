cmake_minimum_required(VERSION 3.8)

project(nes)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_CXX_STANDARD 20)

option(SDL_SHARED "" false)
option(SDL_STATIC "" true)
add_subdirectory(lib/sdl2)

add_subdirectory(lib/nativefiledialog-extended)

add_executable(nes
	src/nes.h
	src/nes.cpp
	src/cpu.cpp
	src/ppu.cpp
	src/apu.cpp
	src/mapper.cpp
	src/main.cpp)

target_link_libraries(nes
	PRIVATE SDL2::SDL2-static
	PRIVATE nfd)

set_property(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	PROPERTY VS_STARTUP_PROJECT nes)
