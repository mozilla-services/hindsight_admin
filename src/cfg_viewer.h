/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Cfg Viewer @file

#ifndef hindsight_admin_cfg_viewer_h_
#define hindsight_admin_cfg_viewer_h_

#ifdef __cplusplus
extern "C"
{
#endif
#include <luasandbox/lua.h>
#ifdef __cplusplus
}
#endif

#include <cstdio>
#include <string>

#include <boost/filesystem.hpp>
#include <Wt/WViewWidget>
#include <Wt/WWidget>

namespace mozilla {
namespace services {
namespace hindsight {

class cfg_viewer : public Wt::WViewWidget {
public:
  void set_filename(const std::string &fn);
  const std::string& get_lua_file() const { return m_lua_file; }

protected:
  virtual Wt::WWidget* renderView();

private:
  std::string m_file;
  std::string m_lua_file;
};


void strip_table(lua_State *cfg, const char **strip);
void output_string(FILE *fh, const char *s);
void output_table(FILE *fh,
                  lua_State *cfg,
                  std::string indent,
                  bool hide);

}
}
}
#endif

