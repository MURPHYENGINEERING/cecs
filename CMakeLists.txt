cmake_minimum_required(VERSION 3.10)

set(ProjectName cecs-example)

project(${ProjectName})

###############################################################################
## file globbing ##############################################################
###############################################################################

# these instructions search the directory tree when CMake is
# invoked and put all files that match the pattern in the variables
# `sources` and `data`
file(GLOB_RECURSE sources      src/*.c src/*.h)
# you can use set(sources src/main.cpp) etc if you don't want to
# use globbing to find files automatically

###############################################################################
## target definitions #########################################################
###############################################################################

# add the data to the target, so it becomes visible in some IDE
add_executable(${ProjectName} ${sources} ${data})

# just for example add some compiler flags
target_compile_options(${ProjectName} PUBLIC -std=c11 -Wextra -Wall -Werror -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wswitch-default -Wswitch-enum -Wconversion -Wno-unused)

# this lets me include files relative to the root source directory with a <> pair
target_include_directories(${ProjectName} PUBLIC include)

# this copies all resource files in the build directory
# we need this, because we want to work with paths relative to the executable
# file(COPY ${data} DESTINATION resources)

###############################################################################
## dependencies ###############################################################
###############################################################################

target_link_libraries(${ProjectName} PUBLIC
  # here you can add any library dependencies
)

###############################################################################
## testing ####################################################################
###############################################################################


###############################################################################
## packaging ##################################################################
###############################################################################

# all install commands get the same destination. this allows us to use paths
# relative to the executable.
install(TARGETS ${ProjectName} DESTINATION example_destination)
