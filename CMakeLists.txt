cmake_minimum_required(VERSION 3.8)
project(ICon6)
cmake_policy(SET CMP0115 OLD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ICON6_BUILD_EXAMPLES "Build examples" ON)
option(ICON6_BUILD_TEST "Build tests" ON)

add_subdirectory(GameNetworkingSockets PRIVATE)

add_subdirectory(bitscpp)

include_directories(./include/)
include_directories(./GameNetworkingSockets/include/)
include_directories(./bitscpp/include/)

aux_source_directory(./include/icon6/ header_files)
aux_source_directory(./src source_files)

add_library(icon6 STATIC
	${header_files}
	${source_files}
)
target_link_libraries(icon6 GameNetworkingSockets::shared)

# if(ICON6_BUILD_TESTS)
# 	add_executable(tests
# 		tests/TestMain
# 	)
# 	target_link_libraries(tests icon6)
# endif()

if(ICON6_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

