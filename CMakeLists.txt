project(gbs-opus)
cmake_minimum_required(VERSION 3.16)

# Minimal build for GBSplay testing in Windows
if (WIN32)
    # Set all targets to build into one directory. Paths are otherwise broken.
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} ) # for MSBuild generators
        string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
        set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR} )
        set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR} )
        set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} )
    endforeach()
endif()


add_subdirectory(lib/SDL)
add_executable(opus src/main.cpp)
add_library(gbs
        lib/libgbs/crc32.c
        lib/libgbs/gbcpu.c
        lib/libgbs/gbhw.c
        lib/libgbs/gbs.c
        lib/libgbs/mapper.c)

target_include_directories(opus PRIVATE lib lib/SDL/include)
target_include_directories(gbs PRIVATE lib/SDL/include)
if (MSVC)
    target_link_libraries(opus PRIVATE SDL2main SDL2 gbs)
    target_link_libraries(gbs PUBLIC zlibstatic)
else()
    target_link_libraries(opus PRIVATE SDL2main SDL2 mingw32 gbs)
    target_link_libraries(gbs PUBLIC z)
endif()







# Link Assets for testing. (Manually copy the folder on build)
include(cmake/link_assets.cmake)

Link_Assets(res)