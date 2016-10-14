
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Admin Registration Model @file

#ifndef hindsight_admin_registration_model_h_
#define hindsight_admin_registration_model_h_

#include <Wt/Auth/AbstractUserDatabase>
#include <Wt/Auth/RegistrationModel>
#include <Wt/Auth/AuthService>
#include <Wt/Auth/Login>
#include <Wt/Auth/Identity>
#include <Wt/WObject>
#include <Wt/WString>

namespace mozilla {
namespace services {
namespace hindsight {

class registration_model : public Wt::Auth::RegistrationModel {
public:
  registration_model(const Wt::Auth::AuthService &auth,
                     Wt::Auth::AbstractUserDatabase &users,
                     Wt::Auth::Login &login,
                     Wt::WObject *parent = 0) :
      Wt::Auth::RegistrationModel(auth, users, login, parent) { }

  Wt::WString validateLoginName(const Wt::WString &name) const override;
  bool registerIdentified(const Wt::Auth::Identity &identity)override;
};

}
}
}
#endif


