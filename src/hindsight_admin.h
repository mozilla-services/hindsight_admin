/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Administration @file

#ifndef hindsight_admin_hindsight_admin_h_
#define hindsight_admin_hindsight_admin_h_

#include <string>

#include <boost/filesystem.hpp>
#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WString>
#include <Wt/WTabWidget>

#include "session.h"

namespace mozilla {
namespace services {
namespace hindsight {

struct hindsight_cfg {
  void load_cfg(const std::string &fn);

  boost::filesystem::path m_hs_install;
  boost::filesystem::path m_hs_run;
  boost::filesystem::path m_hs_load;
  boost::filesystem::path m_hs_output;
  boost::filesystem::path m_plugins;
  boost::filesystem::path m_utilization;
  boost::filesystem::path m_cfg;
  std::string             m_lua_path;
  std::string             m_lua_cpath;
  std::string             m_lua_iopath;
  std::string             m_lua_iocpath;
  int                     m_output_limit;
  int                     m_memory_limit;
  int                     m_instruction_limit;
  int                     m_pm_im_limit;
  int                     m_te_im_limit;
  int                     m_omemory_limit;
  int                     m_oinstruction_limit;
  int                     m_max_message_size;
};


class hindsight_admin : public Wt::WApplication {
public:
  hindsight_admin(const Wt::WEnvironment &env, const hindsight_cfg *cfg);

private:
  mozilla::services::hindsight::session m_session;
  const hindsight_cfg                   *m_hs_cfg;
  Wt::WContainerWidget                  *m_admin;
  Wt::WTabWidget                        *m_tw;
  void onAuthEvent();
  void renderSite();
  bool isPreAuthed(const Wt::WEnvironment &env);
};


template<typename T> Wt::WString tr(T &key)
{
  return Wt::WString::tr(key);
}

}
}
}

#endif

