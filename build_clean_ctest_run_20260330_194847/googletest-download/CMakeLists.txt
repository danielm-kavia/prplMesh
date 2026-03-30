# SPDX-License-Identifier: BSD-3-Clause
#
# SPDX-FileCopyrightText: 2008, Google Inc. (see AUTHORS.md)

# GoogleTest
# https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project

cmake_minimum_required(VERSION 2.8.2)

project(googletest-download NONE)

include(ExternalProject)
ExternalProject_Add(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           v1.10.x
  SOURCE_DIR        "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_194847/googletest-src"
  BINARY_DIR        "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_194847/googletest-build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)
