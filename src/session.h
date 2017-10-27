/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Admin Session @file

#ifndef hindsight_admin_session_h_
#define hindsight_admin_session_h_

#include <vector>
#include <string>

#include <Wt/Auth/Login>
#include <Wt/Dbo/backend/Sqlite3>
#include <Wt/Dbo/ptr>
#include <Wt/Dbo/Session>

#include "user.h"

typedef Wt::Auth::Dbo::UserDatabase<auth_info> user_database;

namespace mozilla {
namespace services {
namespace hindsight {

class session {
public:
  session();
  ~session();

  static void configure_auth();
  static const Wt::Auth::AuthService& auth();
  static const Wt::Auth::AbstractPasswordService& password_auth();
  static const std::vector<const Wt::Auth::OAuthService *>& oauth();

  Wt::Auth::AbstractUserDatabase& users();
  Wt::Auth::Login& login() { return m_login; }

  std::string get_user_name() const;
  const std::string& get_original_name() const;
  void set_user_name(const std::string &name);

private:
  Wt::Dbo::backend::Sqlite3 m_sqlite3;
  mutable Wt::Dbo::Session  m_session;
  user_database             *m_users;
  Wt::Auth::Login           m_login;
  std::string               m_preauth_original_name;
  std::string               m_preauth_user_name;

  Wt::Dbo::ptr<user> get_user() const;
};

std::string normalize_name(const std::string &name);

}
}
}
#endif


