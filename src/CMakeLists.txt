# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(HINDSIGHT_ADMIN_SRC
  auth_widget.cpp
  cfg_viewer.cpp
  constants.cpp
  tester.cpp
  hindsight_admin.cpp
  output_tester.cpp
  plugins.cpp
  registration_model.cpp
  run_matcher.cpp
  session.cpp
  source_viewer.cpp
  user.cpp
  utilization.cpp
)

add_executable(hindsight_admin ${HINDSIGHT_ADMIN_SRC})
target_link_libraries(hindsight_admin
  ${LUASANDBOX_LIBRARIES} 
  ${Wt_LIBRARIES} 
  ${Boost_LIBRARIES}  
  ${UNIX_LIBRARIES})
install(TARGETS hindsight_admin DESTINATION ${CMAKE_INSTALL_BINDIR})

configure_file(constants.in.cpp ${CMAKE_CURRENT_BINARY_DIR}/constants.cpp)
