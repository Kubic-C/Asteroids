cmake_minimum_required(VERSION "3.12")
project("Asteroids" VERSION "0.0.0")

set(BUILD_SHARED_LIBS true)
unset(BUILD_STATIC_LIBS)

if(CMAKE_BUILD_TYPE STREQUAL "Release") 
	message("BUILDING FOR RELEASE")

	if(MSVC)
		# explicitly link to runtime libraries and not debug libraries
		add_compile_options("/MD")
		add_link_options("/MD")
	endif()
endif()

if(UNIX)
	add_compile_options("-Wfatal-errors" "-Wall")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game")

add_subdirectory("deps")
add_subdirectory("src")
