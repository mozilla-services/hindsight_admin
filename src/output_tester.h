/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Output Plugin Tester @file

#ifndef hindsight_admin_output_tester_h_
#define hindsight_admin_output_tester_h_

#include <string>
#include <sstream>

#include <boost/filesystem.hpp>
#include <luasandbox/heka/sandbox.h>
#include <luasandbox/util/heka_message.h>
#include <luasandbox/util/heka_message_matcher.h>
#include <luasandbox/util/protobuf.h>
#include <luasandbox/util/string.h>
#include <Wt/WContainerWidget>
#include <Wt/WMessageBox>
#include <Wt/WTextArea>
#include <Wt/WTreeNode>

#include "hindsight_admin.h"
#include "plugins.h"
#include "run_matcher.h"
#include "session.h"


namespace mozilla {
namespace services {
namespace hindsight {

class output_tester : public Wt::WContainerWidget {
public:
  output_tester(session *s, const hindsight_cfg *hs_cfg, plugins *p);
  ~output_tester();
  void append_log(const char *s);

private:
  Wt::WWidget* result();
  void test_plugin();
  void deploy_plugin();
  void run_matcher();
  void disable_deploy();
  bool test_init();
  Wt::WWidget* message_matcher();
  void finalize();
  void message_box_dismissed();
  void selection_changed(Wt::WString name);

  session             *m_session;
  const hindsight_cfg *m_hs_cfg;
  plugins             *m_plugins;
  Wt::WMessageBox     *m_message_box;
  std::stringstream   m_print;

  // pointers managed by the container
  Wt::WTextArea         *m_cfg;
  Wt::WContainerWidget  *m_msgs;
  Wt::WContainerWidget  *m_debug;
  Wt::WTextArea         *m_logs;
  Wt::WPushButton       *m_deploy;
  Wt::WSelectionBox     *m_selection;
  source_viewer         *m_source;
  // end managed pointers

  Wt::Signals::connection m_cfg_sig;
  struct input_msg m_inputs[g_max_messages];
  int m_inputs_cnt;
};

}
}
}
#endif

