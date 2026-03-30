# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-src"
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-build"
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix"
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix/tmp"
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix/src/googletest-stamp"
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix/src"
  "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix/src/googletest-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix/src/googletest-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/kavia/workspace/code-generation/prplMesh/build_clean_ctest_run_20260330_214500/googletest-download/googletest-prefix/src/googletest-stamp${cfgdir}") # cfgdir has leading slash
endif()
