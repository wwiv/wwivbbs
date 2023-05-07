#
# Common CMake module for WWIV

message(VERBOSE "WWIV Common CMake Module.")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules/sanitizers)
# Need https://github.com/USCiLab/cereal/issues/631 in a build we pull first to move to 20.
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option(WWIV_BUILD_TESTS "Build WWIV test programs" ON)
option(WWIV_SSH_CRYPTLIB "Include support for SSH using Cryptlib" ON)
option(WWIV_ZIP_INSTALL_FILES "Create the zip files for data, gfiles, etc" ON)
option(WWIV_INSTALL "Create install packages for both zip files and binaries." ON)
option(WWIV_USE_PIPES "Enable Named Pipes support for WWIV." ON)

############################################################################### 
#
# Build variables to come from Jenkins, environment, etc.

set(WWIV_RELEASE "5.8.1" CACHE STRING "WWIV Release Major Version to 3 digits")
set(WWIV_BUILD_NUMBER "development" CACHE STRING "WWIV Build Number")
set(WWIV_FULL_RELEASE "${WWIV_RELEASE}.${WWIV_BUILD_NUMBER}" CACHE INTERNAL "WWIV Release Version, 4 digits with build number as 4th")
set(WWIV_ARCH "x86" CACHE STRING "x86 or x64")
set(WWIV_DISTRO "unknown" CACHE STRING "WWIV OS Distribution e.g. (win-x86 | linux-debian10) ")
set(CPACK_PACKAGE_FILE_NAME "wwiv-${WWIV_DISTRO}-${WWIV_FULL_RELEASE}")

set(WWIV_INSTALL_SRC "${CMAKE_SOURCE_DIR}/install" CACHE STRING "By default this is: ${CMAKE_SOURCE_DIR}/install")
set(WWIV_RELEASE_DIR "${CMAKE_BINARY_DIR}/release" CACHE STRING "By default this is: ${CMAKE_BINARY_DIR}/release")
file(MAKE_DIRECTORY ${WWIV_RELEASE_DIR})
#set(MY_CACHE_VARIABLE "VALUE" CACHE STRING "Description")

# Packaging support
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_NAME "WWIV")
set(CPACK_PACKAGE_VENDOR "WWIV Software Services")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WWIV Computer bulletin board system (BBS)")

string(REPLACE "." ";" VERSION_LIST ${WWIV_RELEASE})
list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)

message(STATUS "Set CPACK_PACKAGE_FILE_NAME: ${CPACK_PACKAGE_FILE_NAME}")
message(STATUS "Set CPACK_PACKAGE_VERSION: ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")

file(TO_NATIVE_PATH "C:/wwiv" C_WWIV_PATH)
set(CPACK_NSIS_INSTALL_ROOT "${C_WWIV_PATH}")

set(CPACK_NSIS_PACKAGE_NAME "WWIV BBS Software")
set(CPACK_NSIS_URL_INFO_ABOUT "http://www.wwivbbs.org")
set(CPACK_NSIS_CONTACT "http://docs.wwivbbs.org")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "")
include(CPack)


message(STATUS "WWIV Build Number: ${WWIV_FULL_RELEASE}")


macro(ENSURE_MINIMUM_COMPILER_VERSIONS)
  # Set minimum GCC version
  # See https://stackoverflow.com/questions/14933172/how-can-i-add-a-minimum-compiler-version-requisite
  if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.3)
      message(FATAL_ERROR "Require at least gcc-10.0; found: ${CMAKE_CXX_COMPILER_VERSION}")
  endif()

  if (MSVC)
    if (${MSVC_VERSION} LESS 1932)
      # See https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
      # for versions
      message(FATAL_ERROR "Require at least MSVC 2022 16.2 (1932); Found: ${MSVC_VERSION}")
    endif()
  endif()
endmacro(ENSURE_MINIMUM_COMPILER_VERSIONS)

if (UNIX)
message(STATUS "Platform: UNIX")
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    message(STATUS "Platform: Linux")
    set(LINUX TRUE)
  endif()

elseif (OS2)
  message(STATUS "Platform: OS/2")
  set(CMAKE_CXX_EXTENSIONS OFF)

elseif (WIN32)
  message(STATUS "Platform: WIN32") 

  if (MSVC)
    # Don't show warnings on using normal POSIX functions.  Maybe one day
    # We'll be using all C++ replacements for most things and can get rid of this.
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    add_definitions(/D_CRT_NONSTDC_NO_DEPRECATE)
    
    # Warning 26444 is too noisy to be useful for passing parameters to functions.
    # See https://developercommunity.visualstudio.com/content/problem/422153/warning-c26444-not-aligned-with-cppcoreguidelines.html
    add_definitions(/wd26444)

    # To silence cereal warnings that they know about already
    # bug: https://github.com/USCiLab/cereal/issues/456
    add_definitions(/D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
  endif()
  #
  # Non MSVC Windows Specific settings
  #
  
  # Make Windows.h not so awful if included
  add_definitions(/D_WINSOCK_DEPRECATED_NO_WARNINGS)
  add_definitions(/DNOMINMAX)
  add_definitions(/DWIN32_LEAN_AND_MEAN=1)
  # Otherwise fmt will include windows.h and that breaks everything
  add_definitions(/DFMT_USE_WINDOWS_H=0)

endif()

if(WWIV_USE_PIPES AND (WIN32 OR OS2))
  add_definitions(/DWWIV_USE_PIPES)
endif()

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Debug" )
  message(STATUS "CMAKE_BUILD_TYPE not set; defaulting to Debug")
endif()
message(VERBOSE "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")


macro(set_max_warnings target)
  if(UNIX) 
    target_compile_options("${target}" PRIVATE -Wall -Wextra)
  endif()
  if(WIN32)
    message(VERBOSE "target_compile_options[${target} PRIVATE /W4)]")
    target_compile_options("${target}" PRIVATE /W4)
  endif()
endmacro()

MACRO(MACRO_ENSURE_OUT_OF_SOURCE_BUILD)
  STRING(COMPARE EQUAL "${${PROJECT_NAME}_SOURCE_DIR}"
    "${${PROJECT_NAME}_BINARY_DIR}" insource)
  GET_FILENAME_COMPONENT(PARENTDIR ${${PROJECT_NAME}_SOURCE_DIR} PATH)
  STRING(COMPARE EQUAL "${${PROJECT_NAME}_SOURCE_DIR}"
    "${PARENTDIR}" insourcesubdir)
  IF(insource OR insourcesubdir)
    MESSAGE(FATAL_ERROR 
    "${PROJECT_NAME} requires an out of source build.
     Please see https://github.com/wwivbbs/wwiv#out-of-source-build-warning
     This process created the file `CMakeCache.txt' and the directory `CMakeFiles'.
     Please delete them before re-running cmake."
    )
  ENDIF(insource OR insourcesubdir)
ENDMACRO(MACRO_ENSURE_OUT_OF_SOURCE_BUILD)

  
message(VERBOSE "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

function(zip output_file input_files working_dir)
  #message(DEBUG "zip: ${output_file} : ${input_files}")
  add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${output_file}" --format=zip -- ${input_files}
    WORKING_DIRECTORY "${working_dir}"
    OUTPUT  "${output_file}"
    DEPENDS ${input_files}
    COMMENT "Creating ZIP file: ${output_file}."
    )
endfunction()

function(create_datafile_archive arc dir)
  message(DEBUG "create_datafile_archive: dir: ${dir}: ${WWIV_RELEASE_DIR}/${arc}.zip")
  file(GLOB_RECURSE DATA_FILES "${dir}/*")
  zip("${WWIV_RELEASE_DIR}/${arc}.zip" "${DATA_FILES}" "${dir}/")
  set(ARC_PATH "${WWIV_RELEASE_DIR}/${arc}.zip")
  add_custom_target("${arc}_archive" ALL DEPENDS "${ARC_PATH}")
  install(FILES "${ARC_PATH}" DESTINATION .)
endfunction()

IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  message(VERBOSE "Defining _DEBUG macro for debug build")
  ADD_DEFINITIONS(-D_DEBUG)
ENDIF()
