/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Plugin Administration @file

#ifndef hindsight_admin_plugins_h_
#define hindsight_admin_plugins_h_

#ifdef __cplusplus
extern "C"
{
#endif
#include <luasandbox/lua.h>
#ifdef __cplusplus
}
#endif

#include <string>

#include <boost/filesystem.hpp>
#include <luasandbox.h>
#include <Wt/WPanel>
#include <Wt/WPopupMenu>
#include <Wt/WSortFilterProxyModel>
#include <Wt/WStandardItem>
#include <Wt/WStandardItemModel>
#include <Wt/WTableView>
#include <Wt/WTabWidget>
#include <Wt/WTimer>
#include <Wt/WViewWidget>

#include "cfg_viewer.h"
#include "hindsight_admin.h"
#include "source_viewer.h"

namespace mozilla {
namespace services {
namespace hindsight {

class plugins : public Wt::WContainerWidget {
public:
  plugins(session *s, const hindsight_cfg *cfg);
  ~plugins() { delete m_popup;}

  int find_create(const std::string plugin, lsb_state state);

private:
  Wt::WTableView* create_view();
  void load_run_path(const boost::filesystem::path &dir);
  void load_stats();

  void message_box_dismissed();
  void popup(const Wt::WModelIndex &item, const Wt::WMouseEvent &event);
  void popup_action();
  int add_new_row(const std::string &ptype, const std::string &pname, lsb_state state);

  enum popup_actions {
    cfg,
    lua,
    err,
    stop,
    restart,
    edit,
    del,
  };

  static const int stat_columns = 15;
  session *m_session;
  const hindsight_cfg *m_hs_cfg;

  boost::filesystem::path m_file;
  Wt::WTimer              m_timer;
  Wt::WStandardItemModel  m_stats;
  Wt::WPopupMenu          *m_popup;
  Wt::WMessageBox         *m_message_box;

// pointers managed by the application
  Wt::WSortFilterProxyModel *m_filter;
  Wt::WTableView            *m_view;
  source_viewer             *m_sv;
// end managed pointers
};

}
}
}
#endif

