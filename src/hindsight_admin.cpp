/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Administration Interface @file

#include <stdexcept>

#include <Wt/Auth/AuthWidget>
#include <Wt/WAnchor>
#include <Wt/WApplication>
#include <Wt/WBootstrapTheme>
#include <Wt/WContainerWidget>
#include <Wt/WEnvironment>
#include <Wt/WLink>
#include <Wt/WServer>
#include <Wt/WString>
#include <Wt/WTabWidget>
#include <Wt/WText>

#include "auth_widget.h"
#include "constants.h"
#include "hindsight_admin.h"
#include "output_tester.h"
#include "plugins.h"
#include "session.h"
#include "tester.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <luasandbox/lua.h>
#include <luasandbox/lauxlib.h>
#ifdef __cplusplus
}
#endif

using namespace std;
using namespace Wt;
namespace hs = mozilla::services::hindsight;
namespace fs = boost::filesystem;

static hs::hindsight_cfg g_cfg;


static string get_version()
{
  ostringstream os;
  os << hs::version_major << "." << hs::version_minor << "." << hs::version_patch;
  return os.str();
}


void hs::hindsight_cfg::load_cfg(const string &fn)
{
  m_cfg = fn;
  lua_State *L = nullptr;
  if (!fn.empty()) {
    L = luaL_newstate();
    if (!L) throw runtime_error("luaL_newstate failed");
    int ret = luaL_dofile(L, fn.c_str());
    if (ret) {
      throw runtime_error(lua_tostring(L, -1));
    } else {
      lua_getglobal(L, "sandbox_install_path");
      const char *s = lua_tostring(L, -1);
      if (!s) {
        m_hs_install = "/usr/share/luasandbox/sandboxes/heka";
      } else {
        m_hs_install = s;
      }
      if (!m_hs_install.is_absolute()) {
        m_hs_install = fs::absolute(m_hs_install, m_cfg.parent_path());
      }
      lua_pop(L, 1);

      lua_getglobal(L, "sandbox_run_path");
      m_hs_run = lua_tostring(L, -1);
      if (m_hs_run.empty()) throw runtime_error("invalid hs_cfg no: sandbox_run_path");
      if (!m_hs_run.is_absolute()) {
        m_hs_run = fs::absolute(m_hs_run, m_cfg.parent_path());
      }
      lua_pop(L, 1);

      lua_getglobal(L, "sandbox_load_path");
      m_hs_load = lua_tostring(L, -1);
      if (m_hs_load.empty()) throw runtime_error("invalid hs_cfg no: sandbox_load_path");
      if (!m_hs_load.is_absolute()) {
        m_hs_load = fs::absolute(m_hs_load, m_cfg.parent_path());
      }
      lua_pop(L, 1);

      lua_getglobal(L, "output_path");
      m_hs_output = lua_tostring(L, -1);
      if (m_hs_output.empty()) throw runtime_error("invalid hs_cfg no: output_path");
      if (!m_hs_output.is_absolute()) {
        m_hs_output = fs::absolute(m_hs_output, m_cfg.parent_path());
      }
      lua_pop(L, 1);
      m_stats = m_hs_output / "hindsight.tsv";

      lua_getglobal(L, "analysis_lua_path");
      m_lua_path = lua_tostring(L, -1);
      if (m_lua_path.empty()) throw runtime_error("invalid hs_cfg no: analysis_lua_path");
      lua_pop(L, 1);

      lua_getglobal(L, "analysis_lua_cpath");
      m_lua_cpath = lua_tostring(L, -1);
      if (m_lua_cpath.empty()) throw runtime_error("invalid hs_cfg no: analysis_lua_cpath");
      lua_pop(L, 1);

      lua_getglobal(L, "io_lua_path");
      m_lua_iopath = lua_tostring(L, -1);
      if (m_lua_iopath.empty()) throw runtime_error("invalid hs_cfg no: io_lua_path");
      lua_pop(L, 1);

      lua_getglobal(L, "io_lua_cpath");
      m_lua_iocpath = lua_tostring(L, -1);
      if (m_lua_iopath.empty()) throw runtime_error("invalid hs_cfg no: io_lua_cpath");
      lua_pop(L, 1);

      m_output_limit = -1;
      m_memory_limit = -1;
      m_instruction_limit = -1;
      m_omemory_limit = -1;
      m_oinstruction_limit = -1;
      m_pm_im_limit = 0;
      m_te_im_limit = 10;
      lua_getglobal(L, "analysis_defaults");
      if (lua_type(L, -1) == LUA_TTABLE) {
        lua_getfield(L, -1, "output_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_output_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "memory_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_memory_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "instruction_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_instruction_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "process_message_inject_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_pm_im_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "timer_event_inject_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_te_im_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);
      }
      lua_pop(L, 1);

      lua_getglobal(L, "output_defaults");
      if (lua_type(L, -1) == LUA_TTABLE) {
        lua_getfield(L, -1, "memory_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_omemory_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "instruction_limit");
        if (lua_type(L, -1) == LUA_TNUMBER) {
          m_oinstruction_limit = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
    }
  } else {
    throw runtime_error("no hs_cfg file specified");
  }
  if (L) lua_close(L);
}


static Wt::WApplication* create_application(const Wt::WEnvironment &env)
{
  return new hs::hindsight_admin(env, &g_cfg);
}


int main(int argc, char **argv)
{
  try {
    Wt::WServer server(argc, argv);
    string hs_cfg;
    server.readConfigurationProperty("hs_cfg", hs_cfg);
    g_cfg.load_cfg(hs_cfg);
    server.addEntryPoint(Wt::Application, create_application);
    hs::session::configure_auth();
    server.run();
  } catch (Wt::WServer::Exception &e) {
    std::cerr << e.what() << std::endl;
  } catch (std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
  } catch (...) {
    cerr << "unknown exception" << endl;
  }
}


hs::hindsight_admin::hindsight_admin(const Wt::WEnvironment &env, const hindsight_cfg *cfg) :
    Wt::WApplication(env),
    m_hs_cfg(cfg)
{
  messageResourceBundle().use(WApplication::docRoot() + "/resource_bundle/hindsight_admin");
  useStyleSheet("/css/hindsight_admin.css");
  setCssTheme("polished");
  //setTheme(new Wt::WBootstrapTheme());
  m_session.login().changed().connect(this, &hindsight_admin::onAuthEvent);

  hs::auth_widget *aw = new hs::auth_widget(m_session);

  WText *title = new WText("<h3>Hindsight Admin (" + get_version() + ")</h3>");
  root()->addWidget(title);

  root()->addWidget(aw);

  m_admin = new WContainerWidget(root());
  m_admin->setStyleClass("main");

//  WApplication::instance()->internalPathChanged()
//      .connect(this, &hindsight_admin::ipath_change);

  aw->processEnvironment();
}


void hs::hindsight_admin::onAuthEvent()
{
  if (m_session.login().loggedIn()) {
    m_tw = new Wt::WTabWidget(m_admin);
    hs::plugins *plugins = new hs::plugins(&m_session, m_hs_cfg);
    m_tw->addTab(plugins, tr("tab_plugins"));
    m_tw->addTab(new hs::tester(&m_session, m_hs_cfg, plugins), tr("tab_deploy"));

    std::string val;
    if (Wt::WApplication::instance()->readConfigurationProperty("outputPlugins", val)) {
      m_tw->addTab(new hs::output_tester(&m_session, m_hs_cfg, plugins), tr("tab_output_deploy"));
    }

    Wt::WContainerWidget *lpeg = new Wt::WContainerWidget();
    lpeg->addWidget(new Wt::WText("Grammar Tester - TBD<br/>"));
    Wt::WAnchor *anchor = new Wt::WAnchor(Wt::WLink("http://lpeg.trink.com"), tr("lpeg_grammar_tester"), lpeg);
    anchor->setTarget(Wt::AnchorTarget::TargetNewWindow);
    m_tw->addTab(lpeg, tr("tab_lpeg"));

    Wt::WContainerWidget *dash = new Wt::WContainerWidget();
    dash->addWidget(new Wt::WText("Output Dashboards - TBD<br/>"));
    anchor = new Wt::WAnchor(Wt::WLink("/dashboard_output/"), tr("raw_output"), dash);
    anchor->setTarget(Wt::AnchorTarget::TargetNewWindow);

    anchor = new Wt::WAnchor(Wt::WLink("/dashboard_output/graphs"), tr("graph_output"), dash);
    anchor->setTarget(Wt::AnchorTarget::TargetNewWindow);
    m_tw->addTab(dash, tr("tab_dashboards"));

    Wt::WContainerWidget *footer = new Wt::WContainerWidget(m_admin);
    footer->setStyleClass("copyright");
  } else {
    m_admin->clear();
  }
}
