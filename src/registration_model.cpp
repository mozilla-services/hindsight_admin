/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Admin Registration Model Implementation @file

#include <algorithm>
#include <locale>
#include <string>
#include <sstream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <Wt/Auth/User>
#include <Wt/WApplication>
#include <Wt/WValidator>

#include "registration_model.h"
#include "session.h"

namespace hs = mozilla::services::hindsight;

hs::registration_model::registration_model(const Wt::Auth::AuthService &auth,
                                           Wt::Auth::AbstractUserDatabase &users,
                                           Wt::Auth::Login &login,
                                           Wt::WObject *parent) :
    Wt::Auth::RegistrationModel(auth, users, login, parent)
{
  Wt::WApplication *app = Wt::WApplication::instance();
  std::string val;
  if (app->readConfigurationProperty("authorizedDomains", val)) {
    boost::split(m_auth_domains, val, boost::is_any_of(";"), boost::token_compress_on);
  }
}


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


bool hs::registration_model::validateField(Wt::WFormModel::Field field)
{
  bool validated = Wt::Auth::RegistrationModel::validateField(field);
  if (field == Wt::Auth::RegistrationModel::EmailField && !m_auth_domains.empty()) {
    bool auth_domain = false;
    std::string email = valueText(Wt::Auth::RegistrationModel::EmailField).toUTF8();
    size_t pos = email.find('@');
    if (pos != std::string::npos) {
      if (m_auth_domains.find(email.substr(pos + 1)) != m_auth_domains.end()) {
        auth_domain = true;
      }
    }

    if (auth_domain) {
      setValid(field,  Wt::WString::Empty);
    } else {
      setValidation(field,
                    Wt::WValidator::Result(Wt::WValidator::Invalid,
                                           Wt::WString("unauthorized domain")));
    }
    return auth_domain;
  }
  return validated;
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
    validate();
  }
  return ok;
}
