add_library(lmda-gfx SHARED
  gfx.c
)
add_library(LMDA::Gfx ALIAS lmda-gfx)

target_include_directories(lmda-gfx PUBLIC ${LMDA_INCLUDE_DIR})

target_link_libraries(lmda-gfx
  PUBLIC LMDA::Platform
  PUBLIC SDL3
  PUBLIC cglm_headers
)