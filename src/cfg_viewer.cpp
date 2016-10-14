/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Cfg Viewer Implementation @file

#include "cfg_viewer.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <luasandbox/lauxlib.h>
#ifdef __cplusplus
}
#endif

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <boost/filesystem.hpp>
#include <luasandbox.h>
#include <Wt/WApplication>
#include <Wt/WText>

using namespace std;
namespace hs = mozilla::services::hindsight;
namespace fs = boost::filesystem;

void hs::cfg_viewer::set_filename(const string &fn)
{
  if (!fn.empty()) {
    m_file = fn;
    lua_State *L = luaL_newstate();
    if (!L) return;
    int ret = luaL_dofile(L, m_file.c_str());
    if (ret) {
      lua_close(L);
      return;
    }
    lua_getglobal(L, "filename");
    m_lua_file = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_close(L);

    update(); // trigger rerendering of the view
  }
}


Wt::WWidget* hs::cfg_viewer::renderView()
{
  static const char *strip[] = { "path", "cpath", NULL };
  Wt::WText *result = new Wt::WText();
  result->setMinimumSize(800, 150);
  result->setMaximumSize(800, 150);
  result->setInline(false);

  lua_State *L = luaL_newstate();
  if (!L) {
    Wt::log("error") << "luaL_newstate failed";
    return result;
  }
  int ret = luaL_dofile(L, m_file.c_str());
  if (ret) {
    Wt::log("error") << "dofile failed " <<  m_file << " err: " << lua_tostring(L, -1);
    lua_close(L);
    return result;
  }

  fs::path tmp("/tmp");
  tmp /= Wt::WApplication::instance()->sessionId();
  FILE *fh = fopen(tmp.string().c_str(), "w");
  if (fh) {
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    hs::strip_table(L, strip);
    hs::output_table(fh, L, "", true);
    fclose(fh);
  }
  lua_close(L);

  string sourceHighlightCommand = "source-highlight ";
  sourceHighlightCommand += "--src-lang=lua ";
  sourceHighlightCommand += "--out-format=xhtml ";
  sourceHighlightCommand += "--input=" + tmp.string() + " ";
  tmp.replace_extension("out");
  sourceHighlightCommand += "--output=" + tmp.string();

  bool sourceHighlightOk = system(sourceHighlightCommand.c_str()) == 0;
  if (!sourceHighlightOk) {
    Wt::log("error") << sourceHighlightCommand;
  } else {
    ifstream f(tmp.string().c_str());
    stringstream ss;
    ss << f.rdbuf();
    result->setTextFormat(Wt::XHTMLText);
    result->setText(ss.str());
  }
  remove(tmp);
  tmp.replace_extension();
  remove(tmp);
  return result;
}


void hs::strip_table(lua_State *cfg, const char **strip)
{
  if (!strip) {return;}

  const char **p = strip;
  while (*p) {
    lua_pushnil(cfg);
    lua_setglobal(cfg, *p);
    ++p;
  }
}


void hs::output_string(FILE *fh, const char *s)
{
  size_t len = strlen(s);
  fwrite("'", 1, 1, fh);
  for (unsigned i = 0; i < len; ++i) {
    switch (s[i]) {
    case '\'':
      fwrite("\\'", 2, 1, fh);
      break;
    case '\n':
      fwrite("\\n", 2, 1, fh);
      break;
    case '\r':
      fwrite("\\r", 2, 1, fh);
      break;
    case '\t':
      fwrite("\\t", 2, 1, fh);
      break;
    case '\\':
      fwrite("\\\\", 2, 1, fh);
      break;
    default:
      fwrite(s + i, 1, 1, fh);
      break;
    }
  }
  fwrite("'", 1, 1, fh);
}


void hs::output_table(FILE *fh, lua_State *cfg, string indent, bool hide)
{
  string sep;
  if (!indent.empty()) sep = ",";
  lua_pushnil(cfg);
  while (lua_next(cfg, -2) != 0) {
    int kt = lua_type(cfg, -2);
    int vt = lua_type(cfg, -1);
    switch (kt) {
    case LUA_TNUMBER:
      fprintf(fh, "%s[%d] = ", indent.c_str(), (int)lua_tointeger(cfg, -2));
      break;
    case LUA_TSTRING:
      {
        const char *key = lua_tostring(cfg, -2);
        if (indent.empty()) {
          fprintf(fh, "%s%s = ", indent.c_str(), key);
        } else {
          fprintf(fh, "%s[", indent.c_str());
          hs::output_string(fh, key);
          fwrite("] = ", 4, 1, fh);
        }
        if (hide && key[0] == '_') vt = -1;
      }
      break;
    default:
      break;
    }
    switch (vt) {
    case LUA_TSTRING:
      hs::output_string(fh, lua_tostring(cfg, -1));
      fprintf(fh, "%s\n", sep.c_str());
      break;
    case LUA_TNUMBER:
      fprintf(fh, "%g%s\n", lua_tonumber(cfg, -1), sep.c_str());
      break;
    case LUA_TBOOLEAN:
      fprintf(fh, "%s%s\n", lua_toboolean(cfg, -1) ? "true" : "false", sep.c_str());
      break;
    case LUA_TTABLE:
      fprintf(fh, "{\n");
      hs::output_table(fh, cfg, indent + "  ", hide);
      fprintf(fh, "%s}%s\n", indent.c_str(), sep.c_str());
      break;
    default:
      fprintf(fh, "nil%s -- private\n", sep.c_str());
      break;
    }
    lua_pop(cfg, 1);
  }
}
