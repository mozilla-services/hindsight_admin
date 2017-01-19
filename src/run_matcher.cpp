/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Lua Match Runner @file

#include "run_matcher.h"

#include <string>
#include <sstream>

#include <Wt/WText>
#include <Wt/WTree>
#include <luasandbox/heka/sandbox.h>
#include <luasandbox/util/protobuf.h>

#include "hindsight_admin.h"
#include "session.h"

using namespace std;
namespace fs = boost::filesystem;
namespace hs = mozilla::services::hindsight;

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


static void
output_fields(lsb_heka_message *m, Wt::WTreeNode *n)
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
    n->addChildNode(new Wt::WTreeNode(Wt::WString(ss.str(), Wt::CharEncoding::UTF8)));
  }
}


static ssize_t
read_file(FILE *fh, lsb_input_buffer *ib)
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


static string
get_file_number(const boost::filesystem::path &path) // todo add support for the analysis queue
{
  lua_State *L = luaL_newstate();
  if (!L) {return "0";}

  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setglobal(L, "_G");

  string cpfn((path / "hindsight.cp").string());
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


lua_State* hs::validate_cfg(const std::string &cfg,
                            const std::string &user,
                            lsb_message_matcher **mm,
                            std::string *err_msg)
{
  stringstream err;
  fs::path fn;
  string nfn;
  const char *mms, *tmp;
  int t = 0;
  int v = 0;
  int ret = 0;
  *mm = NULL;

  lua_State *L = luaL_newstate();
  if (!L) {
    err << "luaL_newstate failed";
    goto error;
  }

  ret = luaL_dostring(L, cfg.c_str());
  if (ret) {
    err << lua_tostring(L, -1);
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
  tmp = lua_tostring(L, -1);
  if (!tmp) {
    err << "filename must be specified";
    goto error;
  }
  fn = tmp;
  if (fn.extension() != ".lua") {
    err << "invalid filename extension";
    goto error;
  }
  lua_pop(L, 1);

  nfn = user + "." + hs::normalize_name(fn.stem().string()) + fn.extension().string();
  lua_pushstring(L, nfn.c_str());
  lua_setglobal(L, "filename");

error:
  if (!err.str().empty()) {
    lua_close(L);
    lsb_destroy_message_matcher(*mm);
    *mm = NULL;
    if (err_msg) {
      *err_msg = err.str();
    }
    return NULL;
  }
  return L;
}


size_t hs::run_matcher(const boost::filesystem::path &path,
                       const std::string &cfg,
                       const std::string &user,
                       Wt::WContainerWidget *c,
                       struct hs::input_msg *msgs,
                       size_t msgs_size)
{
  Wt::WTree *tree = new Wt::WTree(c);
  tree->setSelectionMode(Wt::SingleSelection);
  Wt::WTreeNode *root = new Wt::WTreeNode(hs::tr("messages"));
  root->setStyleClass("tree_results");
  tree->setTreeRoot(root);
  root->label()->setTextFormat(Wt::PlainText);
  root->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);

  string err_msg;
  lsb_message_matcher *mm = NULL;
  lua_State *L = validate_cfg(cfg, user, &mm, &err_msg);
  if (!L) {
    return 0;
  }
  lua_close(L);

  fs::path fn = path / "input" / (get_file_number(path) + ".log");
  FILE *fh = fopen(fn.string().c_str(), "rb");
  if (!fh) {
    stringstream ss;
    ss << "could not open the log: " << fn;
    Wt::log("error") << ss.str();
    lsb_destroy_message_matcher(mm);
    return 0;
  }

  size_t cnt = 0;
  ssize_t bytes_read = 1;
  size_t discarded_bytes = 0;
  while (cnt < msgs_size && bytes_read != 0) {
    if (lsb_find_heka_message(&msgs[cnt].m, &msgs[cnt].b, true,
                              &discarded_bytes, NULL)) {
      if (lsb_eval_message_matcher(mm, &msgs[cnt].m)) {
        output_message(&msgs[cnt++].m, root);
      }
    } else {
      bytes_read = read_file(fh, &msgs[cnt].b);
    }
  }
  fclose(fh);
  lsb_destroy_message_matcher(mm);
  root->expand();
  return cnt;
}


void hs::output_message(lsb_heka_message *m, Wt::WTreeNode *root)
{
  if (!m || !root) {return;}

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
    Wt::WTreeNode *f = new Wt::WTreeNode(hs::tr("fields"));
    f->setLoadPolicy(Wt::WTreeNode::NextLevelLoading);
    output_fields(m, f);
    n->addChildNode(f);
  }
}
