/**
 * @file version.h
 * @author Matt Murphy (matt@murphysys.com)
 * @brief Defines the build version; the build system labels output based on this.
 * @date 2024-07-14
 *
 * Copyright (c) 2024 Murphy Engineering LLC
 *
 */
#ifndef LMDA_VERSION_H
#define LMDA_VERSION_H


#define LMDA_VERSION_MAJOR 0
#define LMDA_VERSION_MINOR 0
#define LMDA_VERSION_PATCH 1
#define LMDA_VER_PRERELEASE alpha


#define LMDA_STR(arg) #arg
#define LMDA_XSTR(arg) LMDA_STR(arg)


#define LMDA_VERSION                                                                 \
  "v" LMDA_XSTR(LMDA_VERSION_MAJOR) "." LMDA_XSTR(LMDA_VERSION_MINOR) "." LMDA_XSTR( \
      LMDA_VERSION_PATCH) "-" LMDA_XSTR(LMDA_VER_PRERELEASE)


#endif // LMDA_VERSION_H