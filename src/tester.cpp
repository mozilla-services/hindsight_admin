/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Lua Analysis Plugin Tester @file

#include "tester.h"

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <Wt/WApplication>
#include <Wt/WBootstrapTheme>
#include <Wt/WBreak>
#include <Wt/WContainerWidget>
#include <Wt/WHBoxLayout>
#include <Wt/WLineEdit>
#include <Wt/WMessageBox>
#include <Wt/WNavigationBar>
#include <Wt/WPushButton>
#include <Wt/WText>
#include <Wt/WTextArea>
#include <Wt/WTree>
#include <Wt/WTreeNode>

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
  "thread",
  "process_message_inject_limit",
  "timer_event_inject_limit",

  NULL };
}

void hs::tester::append_log(const char *s)
{
  m_print << s << '\n';
  m_logs->setText(m_print.str());
}


void hs::tester::message_box_dismissed()
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
  hs::tester *t = reinterpret_cast<hs::tester *>(context);
  if (t) {
    t->append_log(output);
  }
}


static int aim(void *parent, const char *pb, size_t pb_len)
{
  hs::tester *t = reinterpret_cast<hs::tester *>(parent);
  if (t->m_im_limit == 0) {
    return 1;
  }
  --t->m_im_limit;
  lsb_heka_message m;
  lsb_init_heka_message(&m, 10);
  lsb_logger logger = { parent, lcb };
  bool ok = lsb_decode_heka_message(&m, pb, pb_len, &logger);
  if (ok) {
    hs::output_message(&m, t->m_itree);
  }
  lsb_free_heka_message(&m);
  return 0;
}


void hs::tester::disable_deploy()
{
  m_deploy->setEnabled(false);
  m_cfg->textInput().disconnect(m_cfg_sig);
  m_sandbox->textInput().disconnect(m_sandbox_sig);
}


void hs::tester::run_matcher()
{
  m_msgs->clear();
  m_injected->clear();
  m_debug->clear();
  string err_msg;
  m_inputs_cnt = hs::run_matcher(m_hs_cfg->m_hs_output,
                                 m_cfg->text().toUTF8(),
                                 m_session->user_name(),
                                 m_msgs,
                                 m_inputs, g_max_messages,
                                 &err_msg);
  if (m_inputs_cnt == 0) {
    Wt::WText *t = new Wt::WText(m_debug);
    if (err_msg.empty()) {
      t->setText(tr("no_matches"));
    } else {
      t->setText(err_msg);
    }
    t->setStyleClass("result_error");
  }
}


Wt::WWidget* hs::tester::message_matcher()
{
  Wt::WContainerWidget *container = new Wt::WContainerWidget();
  container->setStyleClass("message_matcher");
  Wt::WText *t = new Wt::WText(tr("heka_ap_cfg"), container);
  t->setStyleClass("area_title");
  new Wt::WBreak(container);
  m_cfg = new Wt::WTextArea(container);
  m_cfg->setRows(4);
  m_cfg->setText("filename = 'example.lua'\n"
                 "message_matcher = 'TRUE'\n"
                 "preserve_data = false\n"
                 "ticker_interval = 60\n");
  m_cfg_sig = m_cfg->textInput().connect(this, &tester::disable_deploy);

  new Wt::WBreak(container);

  t = new Wt::WText(tr("heka_ap"), container);
  t->setStyleClass("area_title");
  new Wt::WBreak(container);
  m_sandbox = new Wt::WTextArea(container);
  m_sandbox->setRows(15);
  m_sandbox->setText(
      "require \"circular_buffer\"\n"
      "cb = circular_buffer.new(60, 1, 60)\n"
      "cb:set_header(1, \"messages\")\n\n"
      "function process_message()\n"
      "    cb:add(read_message(\"Timestamp\"), 1, 1)\n"
      "    return 0\n"
      "end\n\n"
      "function timer_event(ns, shutdown)\n"
      "    inject_payload(\"cbuf\", \"messages per minute\", cb)\n"
      "end\n"
      );
  m_sandbox_sig = m_sandbox->textInput().connect(this, &tester::disable_deploy);

  Wt::WPushButton *button = new Wt::WPushButton(tr("run_matcher"), container);
  button->clicked().connect(this, &tester::run_matcher);

  button = new Wt::WPushButton(tr("test_plugin"), container);
  button->clicked().connect(this, &tester::test_plugin);

  m_deploy = new Wt::WPushButton(tr("deploy_plugin"), container);
  m_deploy->clicked().connect(this, &tester::deploy_plugin);

  return container;
}


Wt::WWidget* hs::tester::result()
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

  t = new Wt::WText(tr("injected_messages"), c);
  t->setStyleClass("area_title");

  m_injected = new Wt::WContainerWidget(c);
  m_injected->setStyleClass("injected_messages");

  return c;
}


void hs::tester::test_plugin()
{
  m_debug->clear();
  m_injected->clear();
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
  lua_getglobal(L, "filename");
  string fn(lua_tostring(L, -1));
  lua_pop(L, 1);
  lua_close(L);

  fs::path tmp("/tmp");
  tmp /= Wt::WApplication::instance()->sessionId();
  ofstream ofs(tmp.string().c_str());
  if (ofs) {
    ofs << m_sandbox->text();
    ofs.close();
  } else {
    stringstream ss;
    ss << "failed to open: " << tmp;
    Wt::WText *t = new Wt::WText(ss.str(), m_debug);
    t->setStyleClass("result_error");
    Wt::log("error") << ss.str();
    return;
  }

  Wt::WTree *tree = new Wt::WTree(m_injected);
  tree->setSelectionMode(Wt::SingleSelection);
  Wt::WTreeNode *root = new Wt::WTreeNode("Messages");
  root->setStyleClass("tree_results");
  tree->setTreeRoot(root);
  root->label()->setTextFormat(Wt::PlainText);
  root->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);
  m_itree = root;

  lsb_heka_sandbox *hsb;
  lsb_logger logger = { this, lcb };

  std::stringstream cfg;
  cfg << m_cfg->text() << endl;
  cfg << "Hostname = 'test.example.com'\n";
  cfg << "Logger = 'analysis." << fn.substr(0, fn.find_last_of(".")) << "'\n";
  if (m_hs_cfg->m_output_limit >= 0) {
    cfg << "output_limit = " << m_hs_cfg->m_output_limit << "\n";
  }
  if (m_hs_cfg->m_memory_limit >= 0) {
    cfg << "memory_limit = " << m_hs_cfg->m_memory_limit << "\n";
  }
  if (m_hs_cfg->m_instruction_limit >= 0) {
    cfg << "instruction_limit = " << m_hs_cfg->m_instruction_limit << "\n";
  }
  cfg << "process_message_inject_limit = " << m_hs_cfg->m_pm_im_limit << "\n";
  cfg << "timer_event_inject_limit = " << m_hs_cfg->m_te_im_limit << "\n";
  cfg << "path = [[" << m_hs_cfg->m_lua_path << "]]\n";
  cfg << "cpath = [[" << m_hs_cfg->m_lua_cpath << "]]\n";
  cfg << "log_level = 7\n";
  cfg << "Pid = 0\n";

  int rv = 0;
  hsb = lsb_heka_create_analysis(this, tmp.string().c_str(), NULL, cfg.str().c_str(), &logger, aim);
  for (int i = 0; i < m_inputs_cnt; ++i) {
    m_im_limit = m_hs_cfg->m_pm_im_limit;
    rv = lsb_heka_pm_analysis(hsb, &m_inputs[i].m, false);
    if (rv != 0) {
      lcb(this, "", 7, "%s\n", lsb_heka_get_error(hsb));
      break;
    }
  }
  if (rv <= 0) {
    m_im_limit = m_hs_cfg->m_te_im_limit;
    rv = lsb_heka_timer_event(hsb, time(NULL) * 1e9, true);
    if (rv > 0) {
      lcb(this, "", 7, "%s\n", lsb_heka_get_error(hsb));
    }
  }
  lsb_heka_destroy_sandbox(hsb);
  m_itree->expand();
  remove(tmp);
  if (rv <= 0) {
    if (m_deploy->isDisabled()) {
      m_cfg_sig = m_cfg->textInput().connect(this, &tester::disable_deploy);
      m_sandbox_sig = m_sandbox->textInput().connect(this, &tester::disable_deploy);
      m_deploy->setEnabled(true);
    }
  } else {
    disable_deploy();
  }
}


void hs::tester::deploy_plugin()
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

  fs::path sb = m_hs_cfg->m_hs_load / "analysis" / fn;
  fs::path cfg = sb;
  cfg.replace_extension("cfg");
  ofstream ofs(sb.string().c_str());
  if (ofs) {
    ofs << m_sandbox->text();
    ofs.close();
  } else {
    stringstream ss;
    ss << "failed to open: " << sb;
    Wt::WText *t = new Wt::WText(ss.str(), m_debug);
    t->setStyleClass("result_error");
    Wt::log("error") << ss.str();
    lua_close(L);
    return;
  }

  FILE *fh = fopen(cfg.string().c_str(), "w");
  if (fh) {
    strip_table(L, strip_cfg);
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    // simple sdbm hash to consistently distribute the plugins over the
    // available analysis threads
    unsigned long hash = 0;
    const char *str = fn.c_str();
    int c;
    while ((c = *str++)) {
      hash = c + (hash << 6) + (hash << 16) - hash;
    }
    lua_pushinteger(L, (lua_Integer)(hash % 64));
    lua_setglobal(L, "thread");

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

  m_plugins->find_create("analysis." + cfg.stem().string(), LSB_UNKNOWN);
  Wt::WString msg;
  msg = tr("plugin_control").arg(cfg.stem().string()).arg(("deployed"));
  m_message_box = new Wt::WMessageBox(tr("success"), msg, Wt::NoIcon, Wt::Ok);
  m_message_box->buttonClicked().connect(this, &hs::tester::message_box_dismissed);
  m_message_box->show();
}


hs::tester::tester(hs::session *s, const hindsight_cfg *hs_cfg, hs::plugins *p) :
    m_im_limit(0),
    m_session(s),
    m_hs_cfg(hs_cfg),
    m_plugins(p),
    m_inputs_cnt(0)
{
  Wt::WHBoxLayout *hbox = new Wt::WHBoxLayout(this);
  hbox->addWidget(message_matcher(), 1);
  hbox->addWidget(result(), 1);

  for (size_t i = 0; i < g_max_messages; ++i) {
    lsb_init_input_buffer(&m_inputs[i].b, 8000000);
    lsb_init_heka_message(&m_inputs[i].m, 10);
  }
}


hs::tester::~tester()
{
  for (size_t i = 0; i < g_max_messages; ++i) {
    lsb_free_heka_message(&m_inputs[i].m);
    lsb_free_input_buffer(&m_inputs[i].b);
  }
}
