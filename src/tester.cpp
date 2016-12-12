/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Lua Analysis Plugin Tester @file

#include "tester.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <Wt/WApplication>
#include <Wt/WBootstrapTheme>
#include <Wt/WBreak>
#include <Wt/WContainerWidget>
#include <Wt/WHBoxLayout>
#include <Wt/WLineEdit>
#include <Wt/WMenu>
#include <Wt/WMenuItem>
#include <Wt/WMessageBox>
#include <Wt/WNavigationBar>
#include <Wt/WPopupMenu>
#include <Wt/WPopupMenuItem>
#include <Wt/WPushButton>
#include <Wt/WServer>
#include <Wt/WStackedWidget>
#include <Wt/WText>
#include <Wt/WTextArea>
#include <Wt/WTree>
#include <Wt/WTreeNode>
#include <Wt/WVBoxLayout>

#include "constants.h"
#include "plugins.h"

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



void lcb(void *context, const char *component, int level, const char *fmt, ...)
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


int aim(void *parent, const char *pb, size_t pb_len)
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
    t->output_message(&m, NULL);
  }
  lsb_free_heka_message(&m);
  return 0;
}


static const char*
read_string(int wiretype, const char *p, const char *e, lsb_const_string *s)
{
  if (wiretype != LSB_PB_WT_LENGTH) {
    return NULL;
  }

  long long vi;
  p = lsb_pb_read_varint(p, e, &vi);
  if (!p || vi < 0 || p + vi > e) {
    return NULL;
  }
  s->s = p;
  s->len = (size_t)vi;
  return p + vi;
}


static void
output_string_values(const char *p, const char *e, stringstream &ss)
{
  int acnt = 0;
  int tag = 0;
  int wiretype = 0;
  lsb_const_string s;
  while (p && p < e) {
    p = lsb_pb_read_key(p, &tag, &wiretype);
    p = read_string(wiretype, p, e, &s);
    if (p) {
      if (acnt++) ss << ", ";
      ss << "\"" << std::string(s.s, s.len) << "\"";
    }
  }
}


static void
output_integer_values(const char *p, const char *e, stringstream &ss)
{
  int acnt = 0;
  long long ll = 0;
  while (p && p < e) {
    p = lsb_pb_read_varint(p, e, &ll);
    if (p) {
      if (acnt++) ss << ", ";
      ss << ll;
    }
  }
}

static void
output_double_values(const char *p, const char *e, stringstream &ss)
{
  int acnt = 0;
  double d = 0;
  while (p && p < e) {
    memcpy(&d, p, sizeof(double));
    if (acnt++) ss << ", ";
    ss << d;
    p += sizeof(double);
  }
}


void output_fields(lsb_heka_message *m, Wt::WTreeNode *n)
{
  const char *p, *e;
  for (int i = 0; i < m->fields_len; ++i) {
    stringstream ss;
    lsb_heka_field *hf = &m->fields[i];
    ss << std::string(hf->name.s, hf->name.len) << " = ";
    p = hf->value.s;
    e = p + hf->value.len;
    switch (hf->value_type) {
    case LSB_PB_STRING:
    case LSB_PB_BYTES:
      output_string_values(p, e, ss);
      break;
    case LSB_PB_INTEGER:
    case LSB_PB_BOOL:
      output_integer_values(p, e, ss);
      break;
    case LSB_PB_DOUBLE:
      output_double_values(p, e, ss);
      break;
    }
    n->addChildNode(new Wt::WTreeNode(ss.str()));
  }
}


void hs::tester::output_message(lsb_heka_message *m, Wt::WTreeNode *root)
{
  if (!root) {
    root = m_itree;
  }
  stringstream ss;
  ss << "Uuid = ";
  for (unsigned i = 0; i < LSB_UUID_SIZE; ++i) {
    ss << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)((unsigned char)m->uuid.s[i]);
  }
  Wt::WTreeNode *n = new Wt::WTreeNode(ss.str());
  n->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);
  root->addChildNode(n);
  ss.str("");

  char ts[80];
  time_t t = m->timestamp / 1000000000;
  struct tm *tms = gmtime(&t);
  strftime(ts, sizeof ts, "%F %X", tms);
  ss << "Timestamp = " << ts;
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "Type = " << std::string(m->type.s, m->type.len);
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "Logger = " << std::string(m->logger.s, m->logger.len);
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "Severity = " << std::dec << m->severity;
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "Payload = " << std::string(m->payload.s, m->payload.len);
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "EnvVersion = " << std::string(m->env_version.s, m->env_version.len);
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "Pid = ";
  if (m->pid != INT_MIN) {
    ss << std::dec << m->pid;
  }
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  ss.str("");
  ss << "Hostname = " << std::string(m->hostname.s, m->hostname.len);
  n->addChildNode(new Wt::WTreeNode(ss.str()));

  if (m->fields_len) {
    Wt::WTreeNode *f = new Wt::WTreeNode(tr("fields"));
    f->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);
    output_fields(m, f);
    n->addChildNode(f);
  }
}

ssize_t read_file(FILE *fh, lsb_input_buffer *ib)
{
  size_t need;
  if (ib->msglen) {
    need = ib->msglen + (unsigned char)ib->buf[1] + LSB_HDR_FRAME_SIZE
        - ib->readpos; // read exactly to the end of the message
                       // we want one message per buffer since we are saving them off
  } else {
    need = 16; // read enough to grab the header but not part of the next msg
  }

  if (lsb_expand_input_buffer(ib, need)) {
    return -1;
  }
  size_t nread = fread(ib->buf + ib->readpos,
                       1,
                       need,
                       fh);
  if (nread > 0) {
    ib->readpos += nread;
  }
  return nread;
}


string hs::tester::get_file_number()
{
  lua_State *L = luaL_newstate();
  if (!L) {return "0";}

  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setglobal(L, "_G");

  string cpfn((m_hs_cfg->m_hs_output / "hindsight.cp").string());
  int ret = luaL_dofile(L, cpfn.c_str());
  if (ret) {
    Wt::log("error") << "could not parse the checkpoint file: " << cpfn;
    lua_close(L);
    return "0";
  }
  lua_getglobal(L, "input");
  string cp(lua_tostring(L, -1));
  lua_pop(L, 1);
  lua_close(L);

  return cp.substr(0, cp.find_first_of(":"));
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
  m_inputs_cnt = 0;

  Wt::WTree *tree = new Wt::WTree(m_msgs);
  tree->setSelectionMode(Wt::SingleSelection);
  Wt::WTreeNode *root = new Wt::WTreeNode(tr("messages"));
  root->setStyleClass("tree_results");
  tree->setTreeRoot(root);
  root->label()->setTextFormat(Wt::PlainText);
  root->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);

  lsb_message_matcher *mm = NULL;
  lua_State *L = validate_cfg(&mm);
  if (!L) {
    return;
  }
  lua_close(L);

  fs::path fn = m_hs_cfg->m_hs_output / "input" / (get_file_number() + ".log");
  FILE *fh = fopen(fn.string().c_str(), "rb");
  if (!fh) {
    stringstream ss;
    ss << "could not open the log: " << fn;
    Wt::WText *t = new Wt::WText(ss.str(), m_debug);
    t->setStyleClass("result_error");
    Wt::log("error") << ss.str();
    lsb_destroy_message_matcher(mm);
    return;
  }

  int cnt = 0;
  ssize_t bytes_read = 1;
  size_t discarded_bytes = 0;
  while (cnt < g_max_messages && bytes_read != 0) {
    if (lsb_find_heka_message(&m_inputs[cnt].m, &m_inputs[cnt].b, true,
                              &discarded_bytes, NULL)) {
      if (lsb_eval_message_matcher(mm, &m_inputs[cnt].m)) {
        output_message(&m_inputs[cnt++].m, root);
      }
    } else {
      bytes_read = read_file(fh, &m_inputs[cnt].b);
    }
  }
  fclose(fh);
  lsb_destroy_message_matcher(mm);

  m_inputs_cnt = cnt;
  if (cnt == 0) {
    m_logs = new Wt::WTextArea(m_debug);
    m_logs->setText(tr("no_matches"));
  }
  root->expand();
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


lua_State* hs::tester::validate_cfg(lsb_message_matcher **mm)
{
  stringstream err;
  fs::path fn;
  string nfn;
  const char *mms;
  int t = 0;
  int v = 0;
  int ret = 0;
  *mm = NULL;

  lua_State *L = luaL_newstate();
  if (!L) {
    err << "luaL_newstate failed";
    goto error;
  }

  ret = luaL_dostring(L, m_cfg->text().toUTF8().c_str());
  if (ret) {
    err << "malformed cfg";
    goto error;
  }

  lua_getglobal(L, "preserve_data");
  t = lua_type(L, -1);
  if (t != LUA_TNIL && t != LUA_TBOOLEAN) {
    err << "preserve_data must be a nil or boolean";
    goto error;
  }
  lua_pop(L, 1);

  lua_getglobal(L, "ticker_interval");
  t = lua_type(L, -1);
  v = (int)lua_tointeger(L, -1);
  if ((t != LUA_TNIL && t != LUA_TNUMBER) || v < 0) {
    err << "ticker_interval must be a nil or positive number";
    goto error;
  }
  lua_pop(L, 1);

  lua_getglobal(L, "message_matcher");
  mms = lua_tostring(L, -1);
  if (!mms) {
    err << "a message matcher must be specified";
    goto error;
  }
  *mm = lsb_create_message_matcher(mms);
  if (!*mm) {
    err << "invalid message matcher: " << mms;
    goto error;
  }
  lua_pop(L, 1);

  lua_getglobal(L, "filename");
  fn = lua_tostring(L, -1);
  if (fn.extension() != ".lua") {
    err << "invalid filename extension; " << fn.extension();
    goto error;
  }
  lua_pop(L, 1);

  nfn = m_session->user_name() + "." + normalize_name(fn.stem().string())
      + fn.extension().string();
  lua_pushstring(L, nfn.c_str());
  lua_setglobal(L, "filename");

error:
  if (!err.str().empty()) {
    lua_close(L);
    Wt::WText *t = new Wt::WText(err.str(), m_debug);
    t->setStyleClass("result_error");
    Wt::log("error") << err.str();
    lsb_destroy_message_matcher(*mm);
    *mm = NULL;
    return NULL;
  }
  return L;
}


void hs::tester::test_plugin()
{
  m_debug->clear();
  m_injected->clear();
  m_print.str("");

  lsb_message_matcher *mm = NULL;
  lua_State *L = validate_cfg(&mm);
  lsb_destroy_message_matcher(mm);
  if (!L) {
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
  m_logs = new Wt::WTextArea(m_debug);

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

  int rv = 0;
  hsb = lsb_heka_create_analysis(this, tmp.string().c_str(), NULL, cfg.str().c_str(), &logger, aim);
  for (int i = 0; i < m_inputs_cnt; ++i) {
    m_im_limit = m_hs_cfg->m_pm_im_limit;
    rv = lsb_heka_pm_analysis(hsb, &m_inputs[i].m, false);
    if (rv > 0) {
      lcb(this, "", 7, "%s\n", lsb_heka_get_error(hsb));
      break;
    }
  }
  if (rv <= 0) {
    m_im_limit = m_hs_cfg->m_te_im_limit;
    rv = lsb_heka_timer_event(hsb, 0, true);
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

  lsb_message_matcher *mm = NULL;
  lua_State *L = validate_cfg(&mm);
  lsb_destroy_message_matcher(mm);
  if (!L) {
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

  for (int i = 0; i < g_max_messages; ++i) {
    lsb_init_input_buffer(&m_inputs[i].b, 8000000);
    lsb_init_heka_message(&m_inputs[i].m, 10);
  }
}


hs::tester::~tester()
{
  for (int i = 0; i < g_max_messages; ++i) {
    lsb_free_heka_message(&m_inputs[i].m);
    lsb_free_input_buffer(&m_inputs[i].b);
  }
}
