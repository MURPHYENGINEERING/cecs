add_library(cecs SHARED
  ${LMDA_VENDOR_DIR}/cecs/src/cecs.c
)
add_library(CECS::CECS ALIAS cecs)

target_include_directories(cecs PUBLIC ${LMDA_VENDOR_DIR}/cecs/include)