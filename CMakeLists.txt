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

add_subdirectory(bitscpp)

include_directories(./include/)
include_directories(./enet/include/)
include_directories(./bitscpp/include/)

aux_source_directory(./include/icon6/ header_files)
aux_source_directory(./src source_files)

add_library(icon6 STATIC
	${header_files}
	${source_files}
)
# TODO: add code to find if libsodium is installed on system
target_link_libraries(icon6 enet sodium)

add_executable(keygen src/keygen)
target_link_libraries(keygen icon6)

add_executable(certgen src/certgen)
target_link_libraries(certgen icon6)

add_executable(certreq src/certreq)
target_link_libraries(certreq icon6)

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
	add_executable(example2
		examples/example2
	)
	target_link_libraries(example2 icon6)
	add_executable(example3
		examples/example3
	)
	target_link_libraries(example3 icon6)
endif()

