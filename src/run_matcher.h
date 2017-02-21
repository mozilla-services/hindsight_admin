/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Plugin Tester @file

#ifndef hindsight_run_matcher_h_
#define hindsight_run_matcher_h_

#ifdef __cplusplus
extern "C"
{
#endif
#include <luasandbox/lua.h>
#ifdef __cplusplus
}
#endif

#include <string>

#include <Wt/WTreeNode>
#include <Wt/WContainerWidget>
#include <boost/filesystem.hpp>
#include <luasandbox/util/heka_message.h>
#include <luasandbox/util/heka_message_matcher.h>

namespace mozilla {
namespace services {
namespace hindsight {

static const size_t g_max_messages = 5;

struct input_msg {
  lsb_input_buffer b;
  lsb_heka_message m;
};

size_t
run_matcher(const boost::filesystem::path &path,
            const std::string &cfg,
            const std::string &user,
            Wt::WContainerWidget *c,
            struct input_msg *msgs,
            size_t msgs_size,
            std::string *err_msg);

lua_State* validate_cfg(const std::string &cfg, const std::string &user,
                        lsb_message_matcher **mm, std::string *err_msg);

void output_message(lsb_heka_message *m, Wt::WTreeNode *root);

}
}
}
#endif

