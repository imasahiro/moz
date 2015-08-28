include(CheckTypeSize)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckLibraryExists)

cmake_minimum_required(VERSION 2.4)
project(moz)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(VERSION_MAJOR "0")
set(VERSION_MINOR "0")
set(VERSION_PATCH "1")

set(PACKAGE_NAME    ${PROJECT_NAME})
set(PACKAGE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(PACKAGE_STRING  ${CMAKE_PROJECT_NAME}-${PACKAGE_VERSION})
set(SO_VERSION      ${VERSION_MAJOR}.${VERSION_MINOR})
set(PACKAGE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

SET(CFLAGS "-Wsign-compare -Wmissing-field-initializers -Wdisabled-optimization")
SET(CFLAGS "${CFLAGS} -Wall -Wcomment -Wformat -Wunused-macros -march=native")
SET(CMAKE_C_FLAGS         "-Wall -Wno-variadic-macros ${CFLAGS}")
SET(CMAKE_C_FLAGS_RELEASE "-O3 -g")
SET(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
string(TOUPPER ${CMAKE_BUILD_TYPE} uppercase_CMAKE_BUILD_TYPE)

add_definitions(-D__STDC_LIMIT_MACROS)
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_definitions(-DNDEBUG)
endif()

set(NEZ_SRC  src/ast.c src/memo.c src/symtable.c src/node.c src/memory.c)
set(MOZ_SRC  src/loader.c src/main.c src/vm.c)
set(STAT_SRC  src/stat.c)

add_library(nez SHARED ${NEZ_SRC})

add_executable(moz ${MOZ_SRC})
target_link_libraries(moz nez)

add_executable(moz_stat ${STAT_SRC})
add_executable(moz_all ${MOZ_SRC} ${NEZ_SRC})

add_custom_command(OUTPUT vm_core.c vm_inst.h
    COMMAND ruby
    "${CMAKE_CURRENT_SOURCE_DIR}/tool/vmgen.rb"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/instruction.def"
    "${CMAKE_CURRENT_BINARY_DIR}/vm_core.c"
    "${CMAKE_CURRENT_BINARY_DIR}/vm_inst.h"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/instruction.def"
)

add_custom_target(generate_vm_core DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/vm_core.c")

add_dependencies(moz generate_vm_core)

check_type_size("void *" SIZEOF_VOIDP)
check_type_size(long     SIZEOF_LONG)
check_type_size(int      SIZEOF_INT)
check_type_size(float    SIZEOF_FLOAT)
check_type_size(double   SIZEOF_DOUBLE)

check_include_files(gc.h HAVE_GC_H)

set(_HEADERS stdio.h stdlib.h string.h ctype.h)
if(UNIX)
	set(_HEADERS ${_HEADERS} sys/mman.h unistd.h)
endif(UNIX)

if(APPLE)
	set(_HEADERS ${_HEADERS} mach-o/dyld.h)
endif(APPLE)

check_symbol_exists(posix_memalign "${_HEADERS}" HAVE_POSIX_MEMALIGN)
check_symbol_exists(memalign       "${_HEADERS}" HAVE_MEMALIGN)
check_symbol_exists(__builtin_ctzl "${_HEADERS}" HAVE_BUILTIN_CTZL)
check_symbol_exists(bzero "${_HEADERS}" HAVE_BZERO)

add_definitions(-DHAVE_CONFIG_H)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
		${CMAKE_CURRENT_BINARY_DIR}/config.h)

#########################
enable_testing()

add_executable(test_ast    test/test_ast.c)
add_executable(test_objsize test/test_objsize.c)
# add_executable(test_loader test/test_loader.c src/loader.c src/vm.c)
add_executable(test_memo   test/test_memo.c)
add_executable(test_node   test/test_node.c)
add_executable(test_sym    test/test_sym.c)
target_link_libraries(test_ast     nez)
target_link_libraries(test_objsize nez)
target_link_libraries(test_memo    nez)
target_link_libraries(test_node    nez)
target_link_libraries(test_sym     nez)
# target_link_libraries(test_loader nez)
add_test(moz_test_ast     test_ast)
add_test(moz_test_objsize test_objsize)
add_test(moz_test_memo    test_memo)
add_test(moz_test_node    test_node)
add_test(moz_test_sym     test_sym)
# add_test(moz_test_loader test_loader)

install(TARGETS nez LIBRARY DESTINATION lib)

file(GLOB headers ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
foreach(it ${headers})
	install(FILES ${it} DESTINATION include/moz)
endforeach(it ${headers})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vm_inst.h DESTINATION include/moz)

MESSAGE(STATUS)
MESSAGE(STATUS "---------------------------------------------------------------------------" )
MESSAGE(STATUS "CMAKE_BUILD_TYPE     = ${uppercase_CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "CMAKE_C_COMPILER     = ${CMAKE_C_COMPILER}")
MESSAGE(STATUS "CMAKE_CXX_COMPILER   = ${CMAKE_CXX_COMPILER}")
MESSAGE(STATUS "CMAKE_C_FLAGS        = ${CMAKE_C_FLAGS_${uppercase_CMAKE_BUILD_TYPE}}")
MESSAGE(STATUS "CMAKE_CXX_FLAGS      = ${CMAKE_CXX_FLAGS_${uppercase_CMAKE_BUILD_TYPE}}")
MESSAGE(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
MESSAGE(STATUS "Change a value with: cmake -D<Variable>=<Value>" )
MESSAGE(STATUS "---------------------------------------------------------------------------" )
MESSAGE(STATUS)
