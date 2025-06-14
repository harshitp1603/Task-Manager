# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-src")
  file(MAKE_DIRECTORY "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-build"
  "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix"
  "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix/tmp"
  "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix/src/qcustomplot-populate-stamp"
  "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix/src"
  "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix/src/qcustomplot-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix/src/qcustomplot-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/abhis/OneDrive/Desktop/TaskManager/build/Desktop_Qt_6_9_0_MinGW_64_bit-Debug/_deps/qcustomplot-subbuild/qcustomplot-populate-prefix/src/qcustomplot-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
