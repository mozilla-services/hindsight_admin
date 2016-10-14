/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Admin Registration Model Implementation @file

#include <algorithm>
#include <locale>
#include <string>

#include "registration_model.h"
#include "session.h"

namespace hs = mozilla::services::hindsight;

Wt::WString hs::registration_model::validateLoginName(const Wt::WString &name) const
{
  switch (baseAuth()->identityPolicy()) {
  case Wt::Auth::IdentityPolicy::LoginNameIdentity:
    {
      std::string n = name.toUTF8();
      int len = static_cast<int>(n.length());
      bool valid = true;
      for (int i = 0; i < len; ++i) {
        if (!std::islower(n[i]) && !std::isalnum(n[i]) && n[i] != '_') {
          valid = false;
          break;
        }
      }

      if (!valid || len < 3 || len > 20) {
        return Wt::WString::tr("Wt.Auth.user-name-invalid");
      } else {
        return Wt::WString::Empty;
      }
    }

  case Wt::Auth::IdentityPolicy::EmailAddressIdentity:
    if (static_cast<int>(name.toUTF8().length()) < 3
        || name.toUTF8().find('@') == std::string::npos) {
      return Wt::WString::tr("Wt.Auth.email-invalid");
    } else {
      return Wt::WString::Empty;
    }

  default:
    return Wt::WString::Empty;
  }
}


bool hs::registration_model::registerIdentified(const Wt::Auth::Identity &identity)
{
  bool ok = Wt::Auth::RegistrationModel::registerIdentified(identity);
  if (!ok) {
    if (!identity.name().empty()) {
      std::string name = normalize_name(identity.name().toUTF8());
      setValue(LoginNameField,  Wt::WString::fromUTF8(name));
    } else if (!identity.email().empty()) {
      std::string suggested = identity.email();
      std::size_t i = suggested.find('@');
      if (i != std::string::npos) {
        suggested = normalize_name(suggested.substr(0, i));
      }
      setValue(LoginNameField, Wt::WString::fromUTF8(suggested));
    }
  }
  return ok;
}
