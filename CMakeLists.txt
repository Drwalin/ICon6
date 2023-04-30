cmake_minimum_required(VERSION 3.8)
project(ICon6)
cmake_policy(SET CMP0115 OLD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ICON6_BUILD_EXAMPLES "Build examples" ON)
option(ICON6_BUILD_TEST "Build tests" ON)

add_subdirectory(enet)
target_compile_definitions(enet PRIVATE -DENET_DEBUG)

include_directories(./include/)
include_directories(./enet/include/)

aux_source_directory(./include/icon6/ header_files)
aux_source_directory(./src source_files)

# how to integrate autotools with cmake [BEGIN]
# https://stackoverflow.com/questions/5971921/building-a-library-using-autotools-from-cmake
#	cmake_minimum_required(VERSION 2.8)
#	project(test)
#	include(ExternalProject)
#	ExternalProject_Add(libantlr
#		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libantlr
#		CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/libantlr/configure --prefix=<INSTALL_DIR>
#		BUILD_COMMAND ${MAKE})
# how to integrate autotools with cmake [END]

add_library(icon6 STATIC
	${header_files}
	${source_files}
)
target_link_libraries(icon6 enet)

# if(ICON6_BUILD_TESTS)
# 	add_executable(tests
# 		tests/TestMain
# 	)
# 	target_link_libraries(tests icon6)
# endif()

if(ICON6_BUILD_EXAMPLES)
	add_executable(example1
		examples/example1
	)
	target_link_libraries(example1 icon6)
endif()

