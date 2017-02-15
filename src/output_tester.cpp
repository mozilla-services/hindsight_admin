/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Lua Output Plugin Tester @file

#include "output_tester.h"

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <Wt/WApplication>
#include <Wt/WBootstrapTheme>
#include <Wt/WBreak>
#include <Wt/WContainerWidget>
#include <Wt/WHBoxLayout>
#include <Wt/WLineEdit>
#include <Wt/WMessageBox>
#include <Wt/WNavigationBar>
#include <Wt/WPushButton>
#include <Wt/WSelectionBox>
#include <Wt/WText>
#include <Wt/WTextArea>
#include <Wt/WVBoxLayout>

#include "constants.h"

using namespace std;
namespace fs = boost::filesystem;
namespace hs = mozilla::services::hindsight;

namespace {
static const char *strip_cfg[] = {
  // generic sandbox values
  "output_limit",
  "memory_limit",
  "instruction_limit",
  "path",
  "cpath",
  "log_level",

  // Hindsight sandbox values
  // "ticker_interval", // allow
  // "preserve_data", // allow
  "restricted_headers",
  "shutdown_on_terminate",
  "timer_event_inject_limit",

  NULL };
}


void hs::output_tester::append_log(const char *s)
{
  m_print << s << '\n';
  m_logs->setText(m_print.str());
}


void hs::output_tester::message_box_dismissed()
{
  delete m_message_box;
  m_message_box = NULL;
}


static void lcb(void *context, const char *component, int level, const char *fmt, ...)
{
  (void)component;
  (void)level;
  char output[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(output, sizeof output, fmt, args);
  va_end(args);
  hs::output_tester *t = reinterpret_cast<hs::output_tester *>(context);
  if (t) {
    t->append_log(output);
  }
}


static int ucp(void *parent, void *sequence_id)
{
  (void)parent;
  (void)sequence_id;
  return 0;
}


void hs::output_tester::disable_deploy()
{
  m_deploy->setEnabled(false);
  m_cfg->textInput().disconnect(m_cfg_sig);
}


void hs::output_tester::run_matcher()
{
  m_msgs->clear();
  m_debug->clear();
  m_inputs_cnt = hs::run_matcher(m_hs_cfg->m_hs_output,
                                 m_cfg->text().toUTF8(),
                                 m_session->user_name(),
                                 m_msgs,
                                 m_inputs, g_max_messages);

  if (m_inputs_cnt == 0) {
    m_logs = new Wt::WTextArea(m_debug);
    m_logs->setText(tr("no_matches"));
  }
}


Wt::WWidget* hs::output_tester::message_matcher()
{
  Wt::WContainerWidget *container = new Wt::WContainerWidget();
  container->setStyleClass("message_matcher");
  Wt::WText *t = new Wt::WText(tr("heka_op_cfg"), container);
  t->setStyleClass("area_title");
  new Wt::WBreak(container);
  m_cfg = new Wt::WTextArea(container);
  m_cfg->setRows(10);
  m_cfg->setText("filename = 'example.lua'\n"
                 "message_matcher = 'TRUE'\n"
                 "preserve_data = false\n"
                 "ticker_interval = 60\n");
  m_cfg_sig = m_cfg->textInput().connect(this, &output_tester::disable_deploy);

  Wt::WPushButton *button = new Wt::WPushButton(tr("run_matcher"), container);
  button->clicked().connect(this, &output_tester::run_matcher);

  button = new Wt::WPushButton(tr("test_plugin"), container);
  button->clicked().connect(this, &output_tester::test_plugin);

  m_deploy = new Wt::WPushButton(tr("deploy_plugin"), container);
  m_deploy->setEnabled(false);
  m_deploy->clicked().connect(this, &output_tester::deploy_plugin);

  return container;
}


Wt::WWidget* hs::output_tester::result()
{
  Wt::WContainerWidget *c = new Wt::WContainerWidget();
  c->setStyleClass("result_container");

  Wt::WText *t = new Wt::WText(tr("matched_messages"), c);
  t->setStyleClass("area_title");

  m_msgs = new Wt::WContainerWidget(c);
  m_msgs->setStyleClass("matcher_output");

  t = new Wt::WText(tr("debug_output"), c);
  t->setStyleClass("area_title");

  m_debug = new Wt::WContainerWidget(c);
  m_debug->setStyleClass("debug_output");

  auto anchor = new Wt::WAnchor(Wt::WLink("/dashboard_output/output/" + m_session->user_name()), tr("plugin_output_dir"), c);
  anchor->setTarget(Wt::AnchorTarget::TargetNewWindow);

  return c;
}


void hs::output_tester::test_plugin()
{
  m_debug->clear();
  m_print.str("");
  m_logs = new Wt::WTextArea(m_debug);

  string err_msg;
  lsb_message_matcher *mm = NULL;
  lua_State *L = validate_cfg(m_cfg->text().toUTF8(), m_session->user_name(), &mm, &err_msg);
  lsb_destroy_message_matcher(mm);
  if (!L) {
    Wt::WText *t = new Wt::WText(err_msg, m_debug);
    t->setStyleClass("result_error");
    Wt::log("error") << err_msg;
    return;
  }

  lsb_heka_sandbox *hsb;
  lsb_logger logger = { this, lcb };

  std::stringstream cfg;
  cfg << m_cfg->text() << endl;
  cfg << "Hostname = 'test.example.com'\n";
  cfg << "Logger = 'output.test_deploy'\n";
  if (m_hs_cfg->m_omemory_limit >= 0) {
    cfg << "memory_limit = " << m_hs_cfg->m_omemory_limit << "\n";
  }
  if (m_hs_cfg->m_oinstruction_limit >= 0) {
    cfg << "instruction_limit = " << m_hs_cfg->m_oinstruction_limit << "\n";
  }
  cfg << "path = [[" << m_hs_cfg->m_lua_iopath << "]]\n";
  cfg << "cpath = [[" << m_hs_cfg->m_lua_iocpath << "]]\n";
  cfg << "log_level = 7\n";
  std::string val;
  if (!Wt::WApplication::instance()->readConfigurationProperty("outputDir", val)) {
    val = "/var/tmp/hsadmin/output";
  }
  val += "/" + m_session->user_name();
  cfg << "batch_dir = [[" << val << "]]\n";
  cfg << "output_dir = [[" << val << "]]\n";
  cfg << "hindsight_admin = true\n";
  cfg << "Pid = 0\n";

  int rv = 0;
  hsb = lsb_heka_create_output(this, m_source->get_filename().c_str(), NULL,
                               cfg.str().c_str(), &logger, ucp);
  for (int i = 0; i < m_inputs_cnt; ++i) {
    rv = lsb_heka_pm_output(hsb, &m_inputs[i].m, NULL, false);
    if (rv != 0) {
      lcb(this, "", 7, "%s\n", lsb_heka_get_error(hsb));
      break;
    }
  }
  if (rv <= 0) {
    rv = lsb_heka_timer_event(hsb,  time(NULL) * 1e9, true);
    if (rv > 0) {
      lcb(this, "", 7, "%s\n", lsb_heka_get_error(hsb));
    }
  }
  lsb_heka_destroy_sandbox(hsb);
  if (rv <= 0) {
    if (m_deploy->isDisabled()) {
      m_cfg_sig = m_cfg->textInput().connect(this, &output_tester::disable_deploy);
      m_deploy->setEnabled(true);
    }
  } else {
    disable_deploy();
  }
}


void hs::output_tester::deploy_plugin()
{
  static const char ticker_interval[] = "ticker_interval";
  m_debug->clear();

  string err_msg;
  lsb_message_matcher *mm = NULL;
  lua_State *L = validate_cfg(m_cfg->text().toUTF8(), m_session->user_name(), &mm, &err_msg);
  lsb_destroy_message_matcher(mm);
  if (!L) {
    Wt::WText *t = new Wt::WText(err_msg, m_debug);
    t->setStyleClass("result_error");
    Wt::log("error") << err_msg;
    return;
  }

  lua_getglobal(L, "filename");
  string fn(lua_tostring(L, -1));
  lua_pop(L, 1);

  fs::path cfg = m_hs_cfg->m_hs_load / "output" / fn;
  cfg.replace_extension("cfg");

  FILE *fh = fopen(cfg.string().c_str(), "w");
  if (fh) {
    strip_table(L, strip_cfg);
    lua_pushstring(L, m_selection->currentText().toUTF8().c_str());
    lua_setglobal(L, "filename");

    std::string val;
    if (!Wt::WApplication::instance()->readConfigurationProperty("outputDir", val)) {
      val = "/var/tmp/hsadmin/output";
    }
    val += "/" + m_session->user_name();
    lua_pushstring(L, val.c_str());
    lua_setglobal(L, "batch_dir");
    lua_pushstring(L, val.c_str());
    lua_setglobal(L, "output_dir");
    lua_pushboolean(L, 1);
    lua_setglobal(L, "hindsight_admin");

    lua_pushvalue(L, LUA_GLOBALSINDEX);

    // round the ticker_interval up to the next minute
    lua_getglobal(L, ticker_interval);
    lua_Integer li = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_Integer remainder = li % 60;
    if (li == 0 || remainder != 0) {
      lua_pushinteger(L, li + (60 - remainder));
      lua_setglobal(L, ticker_interval);
    }
    output_table(fh, L, "", false);
    fclose(fh);
  }
  lua_close(L);

  m_plugins->find_create("output." + cfg.stem().string(), LSB_UNKNOWN);
  Wt::WString msg;
  msg = tr("plugin_control").arg(cfg.stem().string()).arg(("deployed"));
  m_message_box = new Wt::WMessageBox(tr("success"), msg, Wt::NoIcon, Wt::Ok);
  m_message_box->buttonClicked().connect(this, &hs::output_tester::message_box_dismissed);
  m_message_box->show();
}


void hs::output_tester::selection_changed(Wt::WString name)
{
  boost::filesystem::path lua_file = m_hs_cfg->m_hs_install / "output" / name.toUTF8();
  m_source->set_filename(lua_file.string());
}


hs::output_tester::output_tester(hs::session *s, const hindsight_cfg *hs_cfg, hs::plugins *p) :
    m_session(s),
    m_hs_cfg(hs_cfg),
    m_plugins(p),
    m_inputs_cnt(0)
{

  Wt::WContainerWidget *main = new Wt::WContainerWidget(this);
  Wt::WVBoxLayout *vbox = new Wt::WVBoxLayout();
  main->setLayout(vbox);

  Wt::WHBoxLayout *hbox = new Wt::WHBoxLayout();
  vbox->addLayout(hbox);
  hbox->addWidget(message_matcher(), 1);
  hbox->addWidget(result(), 1);


  Wt::WContainerWidget *c = new Wt::WContainerWidget();
  vbox->addWidget(c);

  new Wt::WBreak(c);
  Wt::WText *t = new Wt::WText(tr("heka_op_plugin"), c);
  t->setStyleClass("area_title");
  new Wt::WBreak(c);

  m_selection = new Wt::WSelectionBox(c);
  new Wt::WBreak(c);
  m_selection->setVerticalSize(2);
  Wt::WApplication *app = Wt::WApplication::instance();
  std::vector<std::string> output_plugins;
  std::string val;
  if (app->readConfigurationProperty("outputPlugins", val)) {
    boost::split(output_plugins, val, boost::is_any_of(";"), boost::token_compress_on);
  }
  for (const std::string &name: output_plugins) {
    m_selection->addItem(name);
  }
  m_source = new source_viewer();
  m_selection->setCurrentIndex(0);
  selection_changed(m_selection->currentText());
  m_selection->sactivated().connect(this, &output_tester::selection_changed);
  new Wt::WBreak(c);
  c->addWidget(m_source);

  for (size_t i = 0; i < g_max_messages; ++i) {
    lsb_init_input_buffer(&m_inputs[i].b, 8000000);
    lsb_init_heka_message(&m_inputs[i].m, 10);
  }
}


hs::output_tester::~output_tester()
{
  for (size_t i = 0; i < g_max_messages; ++i) {
    lsb_free_heka_message(&m_inputs[i].m);
    lsb_free_input_buffer(&m_inputs[i].b);
  }
}
