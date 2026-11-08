# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-src")
  file(MAKE_DIRECTORY "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-build"
  "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix"
  "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix/tmp"
  "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix/src/nanosvg-populate-stamp"
  "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix/src"
  "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix/src/nanosvg-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix/src/nanosvg-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/ARPAN/Work/VibeCoding/cpp-ui-toolkit/build-win/_deps/nanosvg-subbuild/nanosvg-populate-prefix/src/nanosvg-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
