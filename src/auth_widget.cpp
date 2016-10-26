/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Admin Auth Widget Implementation @file

#include <string>

#include <Wt/WApplication>

#include "auth_widget.h"
#include "registration_model.h"


namespace hs = mozilla::services::hindsight;

hs::auth_widget::auth_widget(hs::session &s) :
    Wt::Auth::AuthWidget(s.login()),
    m_session(s)
{
  Wt::Auth::AuthModel *am = new Wt::Auth::AuthModel(s.auth(), s.users(), this);
  am->addPasswordAuth(&hs::session::password_auth());
  am->addOAuth(hs::session::oauth());
  setModel(am);

  Wt::WApplication *app = Wt::WApplication::instance();
  std::string val;
  if (app->readConfigurationProperty("enableRegistration", val)) {
    if (val == "true") {
      setRegistrationEnabled(true);
    } else {
      setRegistrationEnabled(false);
    }
  }
}


Wt::Auth::RegistrationModel* hs::auth_widget::createRegistrationModel()
{
  return new hs::registration_model(m_session.auth(),
                                    m_session.users(),
                                    m_session.login(),
                                    this);
}
