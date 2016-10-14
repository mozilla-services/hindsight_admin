/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight User @file

#ifndef hindsight_admin_user_h_
#define hindsight_admin_user_h_

#include <string>

#include <Wt/Dbo/Types>
#include <Wt/Dbo/WtSqlTraits>
#include <Wt/Auth/Dbo/AuthInfo>

class user;
typedef Wt::Auth::Dbo::AuthInfo<user> auth_info;
typedef Wt::Dbo::collection<Wt::Dbo::ptr<user> > users;

class user {
public:
  user();

  std::string name; /* a copy of auth info's user name */
  Wt::Dbo::collection<Wt::Dbo::ptr<auth_info> > auth_infos;

  template<class Action>
  void persist(Action &a)
  {
    Wt::Dbo::hasMany(a, auth_infos, Wt::Dbo::ManyToOne, "user");
  }
};


DBO_EXTERN_TEMPLATES(user)

#endif
