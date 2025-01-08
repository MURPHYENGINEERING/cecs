cmake_minimum_required(VERSION 3.10)

message("CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES: ${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES}")

include(cmake/version.cmake)

project(lmda
  VERSION ${LMDA_VERSION}
  DESCRIPTION "Lagrange Marine Defense Agency"
  HOMEPAGE_URL "https://lmda.murphysys.com/"
  LANGUAGES C
)

set_property(GLOBAL PROPERTY C_STANDARD 11)

include(cmake/detect_buildtype.cmake) 
message("-----------------------------------")

set(LMDA_CMAKE_DIR "${PROJECT_SOURCE_DIR}/cmake")
set(LMDA_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")

# Third-party libraries
set(LMDA_VENDOR_DIR "${PROJECT_SOURCE_DIR}/lib")
if(WIN32)
  set(LMDA_VENDOR_PLATFORM_DIR "${LMDA_VENDOR_DIR}/win")
else()
  set(LMDA_VENDOR_PLATFORM_DIR "${LMDA_VENDOR_DIR}/mac")
endif()

list(APPEND LMDA_INCLUDE_DIR "${LMDA_VENDOR_DIR}/cecs/include")

set(CMAKE_BIN_DIR "${PROJECT_SOURCE_DIR}/bin/${EX_PLATFORM_NAME}-${CMAKE_BUILD_TYPE}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR})

set(LMDA_ASSET_DIR        "${PROJECT_SOURCE_DIR}/asset")
set(LMDA_ASSET_OUTPUT_DIR "${CMAKE_BIN_DIR}/asset")

if(DEBUG)
  add_compile_definitions(DEBUG)
endif()

if (WIN32)

# TODO: fix for C
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  set(CMAKE_C_FLAGS "-DWIN32 -D_WIN32_WINNT=0x601 -DWIN32_LEAN_AND_MEAN")
  add_compile_definitions(
    WIN32
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x601
  )
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

else ()

  # Don't try to look in weird system places for standard libs
  set(CMAKE_OSX_SYSROOT "")
  # This was supposed to fix something that wasn't actually broken. Now it just
  # seems sensible.
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  link_libraries(Threads::Threads)

endif ()

add_compile_definitions(
  LMDA_LOG_FILE_NAME="lmda.log"
  LMDA_LOG_MAX_KB=10
  ## https://github.com/rxi/log.c#log_use_color
  #LOG_USE_COLOR
)

###############################################################################
## file globbing ##############################################################
###############################################################################

# these instructions search the directory tree when CMake is
# invoked and put all files that match the pattern in the variables
# `sources` and `data`
file(GLOB_RECURSE sources      src/*.c include/cecs/*.h)
# you can use set(sources src/main.cpp) etc if you don't want to
# use globbing to find files automatically


link_directories("${LMDA_VENDOR_DIR}/SDL/build/debug")

###############################################################################
## target definitions #########################################################
###############################################################################
add_subdirectory(src)

# add the data to the target, so it becomes visible in some IDE
# just for example add some compiler flags

# this copies all resource files in the build directory
# we need this, because we want to work with paths relative to the executable
# file(COPY ${data} DESTINATION resources)

###############################################################################
## dependencies ###############################################################
###############################################################################
include(${LMDA_CMAKE_DIR}/sdl.cmake)
include(${LMDA_CMAKE_DIR}/cglm.cmake)


###############################################################################
## testing ####################################################################
###############################################################################


###############################################################################
## packaging ##################################################################
###############################################################################

# all install commands get the same destination. this allows us to use paths
# relative to the executable.
#install(TARGETS lmda DESTINATION example_destination)
