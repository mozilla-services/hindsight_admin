/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Source Viewer Implementation @file

#include "source_viewer.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <boost/filesystem.hpp>
#include <Wt/WApplication>
#include <Wt/WText>

using namespace std;
namespace hs = mozilla::services::hindsight;
namespace fs = boost::filesystem;

void hs::source_viewer::set_filename(const string &fn)
{
  if (!fn.empty()) {
    m_file = fn;
    update(); // trigger rerendering of the view
  }
}


Wt::WWidget* hs::source_viewer::renderView()
{
  Wt::WText *result = new Wt::WText();
  result->setMinimumSize(800, 400);
  result->setMaximumSize(800, 400);
  result->setInline(false);

  fs::path tmp("/tmp");
  tmp /= Wt::WApplication::instance()->sessionId();
  string sourceHighlightCommand = "source-highlight ";
  sourceHighlightCommand += "--src-lang=lua ";
  sourceHighlightCommand += "--out-format=xhtml ";
  sourceHighlightCommand += "--input=" + m_file + " ";
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
  return result;
}
