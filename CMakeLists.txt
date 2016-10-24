# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(hindsight_admin VERSION 0.0.3)
set(CPACK_PACKAGE_NAME hindsight-admin)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Hindsight Administration Interface")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR        "Mozilla Services")
set(CPACK_PACKAGE_CONTACT       "Mike Trinkala <trink@mozilla.com>")
set(CPACK_STRIP_FILES           TRUE)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_RPM_PACKAGE_LICENSE    "MPLv2.0")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "witty (>=3.3.4), source-highlight (>=3.1), luasandbox (>=1.2), hindsight (>= 0.12.5)")

set(PROJECT_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(mozsvc)
include(GNUInstallDirs)

if(CMAKE_HOST_UNIX)
  find_library(PTHREAD_LIBRARY pthread)
  find_library(NSL_LIBRARY nsl)
  find_library(OPENSSL_LIBRARY ssl)
  find_library(CRYPTO_LIBRARY crypto)
  find_library(RT_LIBRARY rt)
  find_library(LIBZ_LIBRARY z)
  find_library(LIBDL_LIBRARY dl)

  set(UNIX_LIBRARIES
  ${PTHREAD_LIBRARY}
  ${NSL_LIBRARY}
  ${OPENSSL_LIBRARY}
  ${CRYPTO_LIBRARY}
  ${RT_LIBRARY}
  ${LIBZ_LIBRARY}
  ${LIBDL_LIBRARY})
endif()

find_package(Boost 1.53.0 REQUIRED
date_time
filesystem
program_options
random regex
thread
signals
system unit_test_framework)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Wt REQUIRED)
include_directories(${Wt_INCLUDE_DIR})
set(WT_CONFIG_NAME ${PROJECT_NAME}_server.xml)
set(WT_CONFIG "/etc/wt/${WT_CONFIG_NAME}")

find_package(luasandbox 1.2 REQUIRED CONFIG)

include_directories("${PROJECT_PATH}/include")

add_subdirectory(src)
install(FILES "${WT_CONFIG_NAME}" DESTINATION /etc/wt)
install(DIRECTORY resource_bundle DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
install(DIRECTORY resources DESTINATION  ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
install(DIRECTORY css DESTINATION  ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
install(PROGRAMS run.sh DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
