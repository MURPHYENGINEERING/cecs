add_executable(lmda-host
  main.c
  ${log_config_file_output}
)

target_compile_options(lmda-host PUBLIC -std=c11 -Wextra -Wall -Werror -Wundef -Wshadow -Wpointer-arith -Wswitch-enum -Wconversion -Wno-unused)


target_include_directories(lmda-host PUBLIC ${LMDA_INCLUDE_DIR})

target_link_libraries(lmda-host
  PUBLIC CECS::CECS
  PUBLIC LMDA::Platform
  PUBLIC LMDA::Game
  PUBLIC LMDA::Gfx
)