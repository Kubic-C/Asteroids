
add_executable(asteroids 
	"main.cpp" "base.hpp" "game.hpp" "game.cpp" "component.hpp" "statesDefine.hpp" "statesDefine.cpp")

target_link_libraries(asteroids PUBLIC 
	AsteroidsEngine)

add_custom_command(TARGET asteroids POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:asteroids> $<TARGET_FILE_DIR:asteroids>
  COMMAND_EXPAND_LISTS
  )

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	if (WIN32)
		target_link_options(asteroids PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
	else()
		target_link_options(asteroids PRIVATE "-mwindows")
	endif()
endif()