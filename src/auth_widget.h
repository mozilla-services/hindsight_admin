/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Admin Auth Widget @file

#ifndef hindsight_admin_auth_widget_h_
#define hindsight_admin_auth_widget_h_

#include "registration_model.h"
#include "session.h"

#include <Wt/Auth/AbstractUserDatabase>
#include <Wt/Auth/AuthService>
#include <Wt/Auth/AuthWidget>
#include <Wt/Auth/RegistrationModel>
#include <Wt/WContainerWidget>

namespace mozilla {
namespace services {
namespace hindsight {

class auth_widget : public Wt::Auth::AuthWidget {
public:
  auth_widget(session &s);

protected:
  virtual Wt::Auth::RegistrationModel* createRegistrationModel() override;

private:
  session &m_session;
};

}
}
}
#endif
