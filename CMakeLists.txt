cmake_minimum_required(VERSION 3.2.2)

if(${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

project (libsocket)
set(CMAKE_BUILD_TYPE Release)

# Directory variables
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Use C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Warnings 
if(MSVC)
	# Force to always compile with W4
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGCC OR CMAKE_COMPILER_IS_CLANGCXX)
	# Update if necessary
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default \
		-Wswitch-enum -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline \
		-Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow -Wnon-virtual-dtor \
		-Wconversion")
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wzero-as-null-pointer-constant")
	endif()
endif()

#Generate the static library from the sources
add_library(socket STATIC ${SOURCE_DIR}/socket.cpp)

#Install the library in the system
install(TARGETS socket DESTINATION lib)
install(FILES socket.hpp DESTINATION include)