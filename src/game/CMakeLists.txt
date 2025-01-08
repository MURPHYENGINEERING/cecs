add_library(lmda-game SHARED
  game.c
)
add_library(LMDA::Game ALIAS lmda-gfx)

target_include_directories(lmda-game PUBLIC ${LMDA_INCLUDE_DIR})

target_link_libraries(lmda-game
  PUBLIC LMDA::Platform
  PUBLIC cglm_headers
)