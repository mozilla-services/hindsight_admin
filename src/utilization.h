/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Utilization @file

#ifndef hindsight_admin_utilization_h_
#define hindsight_admin_utilization_h_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <Wt/WContainerWidget>
#include <Wt/WSortFilterProxyModel>
#include <Wt/WStandardItemModel>
#include <Wt/WTreeView>
#include <Wt/WTimer>

#include "hindsight_admin.h"

namespace mozilla {
namespace services {
namespace hindsight {

class utilization : public Wt::WContainerWidget {
public:
  utilization(session *s, const hindsight_cfg *cfg);

private:
  Wt::WTreeView* create_view();
  void load_stats();

  Wt::WStandardItem* find_row(Wt::WStandardItem *parent, const std::string &name, int row);

  Wt::WStandardItem* add_row(Wt::WStandardItem *parent, int row, const std::vector<std::string> &cols);

  Wt::WStandardItem* insert_row(Wt::WStandardItem *parent, int row, const std::vector<std::string> &cols);

  void update_row(Wt::WStandardItem *parent, int row, const std::vector<std::string> &cols);

  static const int stat_columns = 6;
  session *m_session;
  const hindsight_cfg *m_hs_cfg;

  boost::filesystem::path m_file;
  Wt::WTimer              m_timer;
  Wt::WStandardItemModel  m_stats;

// pointers managed by the application
  Wt::WSortFilterProxyModel *m_filter;
  Wt::WTreeView             *m_view;
// end managed pointers
};

}
}
}
#endif

