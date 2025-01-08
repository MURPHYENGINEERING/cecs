add_library(lmda-platform
  common/log.c
  common/fs.c
  common/timing.c
)


target_include_directories(lmda-platform PUBLIC ${LMDA_INCLUDE_DIR})


if(WIN32)
  target_sources(lmda-platform PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/win/fs.c
    ${CMAKE_CURRENT_LIST_DIR}/win/timing.c
    ${CMAKE_CURRENT_LIST_DIR}/win/udp.c
  )
else()
  target_sources(lmda-platform PRIVATE
    mac/fs.c
    mac/timing.c
    mac/udp.c
  )
endif()

add_library(LMDA::Platform ALIAS lmda-platform)


target_link_libraries(lmda-platform
  #PUBLIC SDL2::SDL2
)