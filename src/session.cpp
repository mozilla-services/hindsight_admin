/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Session Implementation @file

#include "session.h"

#include <algorithm>
#include <locale>
#include <string>
#include <unistd.h>

#include <Wt/Auth/AuthService>
#include <Wt/Auth/Dbo/AuthInfo>
#include <Wt/Auth/Dbo/UserDatabase>
#include <Wt/Auth/GoogleService>
#include <Wt/Auth/HashFunction>
#include <Wt/Auth/PasswordService>
#include <Wt/Auth/PasswordStrengthValidator>
#include <Wt/Auth/PasswordVerifier>
#include <Wt/WApplication>
#include <Wt/WLogger>

using namespace Wt;
namespace dbo = Wt::Dbo;
namespace hs = mozilla::services::hindsight;

namespace {
class oauth_services : public std::vector<const Auth::OAuthService *> {
public:
  ~oauth_services()
  {
    for (unsigned i = 0; i < size(); ++i) delete (*this)[i];
  }
};

Auth::AuthService g_auth_service;
Auth::PasswordService g_password_service(g_auth_service);
oauth_services g_oauth_services;
}


std::string hs::session::user_name() const
{
  if (m_login.loggedIn()) {
    return m_login.user().identity(Auth::Identity::LoginName).toUTF8();
  }
  return "UNKNOWN";
}


void hs::session::configure_auth()
{
  g_auth_service.setAuthTokensEnabled(true, "hindsight_admin");
  g_auth_service.setEmailVerificationRequired(true);

  Auth::PasswordVerifier *verifier = new Auth::PasswordVerifier();
  verifier->addHashFunction(new Auth::BCryptHashFunction(7));

  g_password_service.setVerifier(verifier);
  g_password_service.setStrengthValidator(new Auth::PasswordStrengthValidator());
  g_password_service.setAttemptThrottlingEnabled(true);

  if (Auth::GoogleService::configured()) {
    g_oauth_services.push_back(new Auth::GoogleService(g_auth_service));
  }
}


hs::session::session()
    : m_sqlite3(WApplication::instance()->appRoot() + "hindsight_admin.db")
{
  m_session.setConnection(m_sqlite3);
  m_sqlite3.setProperty("show-queries", "true");

  m_session.mapClass<user>("user");
  m_session.mapClass<auth_info>("auth_info");
  m_session.mapClass<auth_info::AuthIdentityType>("auth_identity");
  m_session.mapClass<auth_info::AuthTokenType>("auth_token");

  m_users = new user_database(m_session);

  dbo::Transaction transaction(m_session);
  try {
    m_session.createTables();
    Wt::WApplication *app = WApplication::instance();
    std::string val;
    if (app->readConfigurationProperty("enableGuest", val)) {
      if (val == "true") {
        Auth::User guest_user = m_users->registerNew();
        guest_user.addIdentity(Auth::Identity::LoginName, "guest");
        guest_user.setEmail("guest@example.com");
        g_password_service.updatePassword(guest_user, "guest");
      }
    }
    Wt::log("info") << "Database created";
  } catch (...) {
    Wt::log("info") << "Using existing database";
  }
  transaction.commit();
}


hs::session::~session()
{
  delete m_users;
}


dbo::ptr<user> hs::session::get_user() const
{
  if (m_login.loggedIn()) {
    dbo::ptr<auth_info> auth_info = m_users->find(m_login.user());
    dbo::ptr<user> u = auth_info->user();

    if (!u) {
      u = m_session.add(new user());
      auth_info.modify()->setUser(u);
    }
    return u;
  } else {
    return dbo::ptr<user>();
  }
}


Auth::AbstractUserDatabase& hs::session::users()
{
  return *m_users;
}


const Auth::AuthService& hs::session::auth()
{
  return g_auth_service;
}


const Auth::AbstractPasswordService& hs::session::password_auth()
{
  return g_password_service;
}


const std::vector<const Auth::OAuthService *>& hs::session::oauth()
{
  return g_oauth_services;
}


static char lower_alnum(char c)
{
  return std::isalnum(c) ? std::tolower(c) : '_';
}


std::string hs::normalize_name(const std::string &name)
{
  std::string normal(name);
  std::transform(normal.begin(), normal.end(), normal.begin(), lower_alnum);
  return normal;
}
