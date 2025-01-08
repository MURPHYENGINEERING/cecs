if (WIN32)

  #set(SDL2_PATH        "${LMDA_VENDOR_PLATFORM_DIR}/SDL2")
  #set(SDL2_LIBRARY     "${SDL2_PATH}/lib/x64/SDL2.lib")
  #set(SDL2_INCLUDE_DIR "${SDL2_PATH}/include")
  #set(SDL2_TTF_PATH    "${LMDA_VENDOR_PLATFORM_DIR}/SDL2_ttf")
  #set(SDL2_IMAGE_PATH  "${LMDA_VENDOR_PLATFORM_DIR}/SDL2_image")

endif ()

# Import the FindSDL utilities
#list(APPEND CMAKE_MODULE_PATH ${LMDA_CMAKE_DIR}/sdl2)

list(APPEND CMAKE_PREFIX_PATH ${LMDA_VENDOR_DIR}/SDL/build/debug)

find_package(SDL3 REQUIRED)
#find_package(SDL2_image REQUIRED)
#find_package(SDL2_ttf REQUIRED)
#find_package(SDL2_mixer REQUIRED)


#add_library(SDL3_BUNDLE INTERFACE)
#add_library(SDL3::SDL2 ALIAS SDL2_BUNDLE)