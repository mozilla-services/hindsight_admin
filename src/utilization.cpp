/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Utilization Implementation @file

#include "utilization.h"

#include <fstream>
#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <Wt/WStandardItem>

#include "constants.h"

using namespace std;
namespace hs = mozilla::services::hindsight;
namespace fs = boost::filesystem;

hs::utilization::utilization(hs::session *s, const hindsight_cfg *cfg) :
    m_session(s),
    m_hs_cfg(cfg),
    m_stats(0, stat_columns)
{
  m_stats.setHeaderData(0, Wt::Horizontal, tr("plugin_name"));
  m_stats.setHeaderData(1, Wt::Horizontal, tr("messages_processed"));
  m_stats.setHeaderData(2, Wt::Horizontal, tr("%utilization"));
  m_stats.setHeaderData(3, Wt::Horizontal, tr("%message_matcher"));
  m_stats.setHeaderData(4, Wt::Horizontal, tr("%process_message"));
  m_stats.setHeaderData(5, Wt::Horizontal, tr("%timer_event"));
  this->addWidget(create_view());
}


Wt::WStandardItem* hs::utilization::find_row(Wt::WStandardItem *parent, const string &name, int row)
{
  int rows = parent->rowCount();
  for (; row < rows; ++row) {
    Wt::WStandardItem *item = parent->child(row, 0);
    string tmp(boost::any_cast<string>(item->data(Wt::DisplayRole)));
    if (tmp == name) {
      return item;
    }
  }
  return nullptr;
}


static void set_utilization(Wt::WStandardItem *tmp, int val)
{
  if (val < 0) {
    // no style
  } else if (val < 70) {
    tmp->setStyleClass("utilization_low");
  } else if (val < 90) {
    tmp->setStyleClass("utilization_medium");
  } else if (val < 100) {
    tmp->setStyleClass("utilization_high");
  } else {
    tmp->setStyleClass("utilization_excessive");
  }
}


Wt::WStandardItem* hs::utilization::add_row(Wt::WStandardItem *parent, int row, const vector<string> &cols)
{
  Wt::WStandardItem *item = nullptr;
  int rows = parent->rowCount();
  if (row >= rows) {
    item = new Wt::WStandardItem();
    item->setData(cols[0], Wt::DisplayRole);
    parent->setChild(row, 0, item);
    for (int col = 1; col < stat_columns; ++col) {
      Wt::WStandardItem *tmp = new Wt::WStandardItem();
      int val = boost::lexical_cast<int>(cols[col]);
      tmp->setData(val, Wt::DisplayRole);
      if (col == 2 && parent ==  m_stats.invisibleRootItem()) {
        set_utilization(tmp, val);
      }
      parent->setChild(row, col, tmp);
    }
  }
  return item;
}


Wt::WStandardItem* hs::utilization::insert_row(Wt::WStandardItem *parent, int row, const vector<string> &cols)
{
  Wt::WStandardItem *item = new Wt::WStandardItem();
  item->setData(cols[0], Wt::DisplayRole);
  parent->insertRow(row, item);
  for (int col = 1; col < stat_columns; ++col) {
    Wt::WStandardItem *tmp = new Wt::WStandardItem();
    int val = boost::lexical_cast<int>(cols[col]);
    tmp->setData(val, Wt::DisplayRole);
    if (col == 2 && parent ==  m_stats.invisibleRootItem()) {
      set_utilization(tmp, val);
    }
    parent->setChild(row, col, tmp);
  }
  return item;
}


void hs::utilization::update_row(Wt::WStandardItem *parent, int row, const vector<string> &cols)
{
  parent->child(row, 0)->setData(cols[0], Wt::DisplayRole);
  for (int col = 1; col < stat_columns; ++col) {
    Wt::WStandardItem *tmp = parent->child(row, col);
    int val = boost::lexical_cast<int>(cols[col]);
    tmp->setData(val, Wt::DisplayRole);
    if (col == 2 && parent ==  m_stats.invisibleRootItem()) {
      set_utilization(tmp, val);
    }
  }
}


void hs::utilization::load_stats()
{
  if (m_hs_cfg->m_utilization.empty()) {
    return;
  }

  bool header = true;
  int cnt     = 0;
  string s;
  ifstream ifs(m_hs_cfg->m_utilization.string().c_str());
  vector<string> cols;

  Wt::WStandardItem *root = m_stats.invisibleRootItem();
  Wt::WStandardItem *parent = nullptr;
  int row  = 0;
  int rowp = 0;

  while (getline(ifs, s).good()) {
    boost::split(cols, s, boost::is_any_of("\t"));
    cnt = cols.size();
    if (cnt < stat_columns) continue;

    if (header) {
      header = false;
      continue;
    }

    if (boost::starts_with(cols[0], "analysis.")) {
      if (!parent) {
        m_stats.clear();
        return; // invalid input
      }
      if (!add_row(parent, rowp, cols)) {
        update_row(parent, rowp, cols);
      }
      ++rowp;
    } else {
      if (parent) {
        if (parent->rowCount() > rowp) {
          parent->removeRows(rowp, parent->rowCount() - rowp);
        }
        parent = nullptr;
      }
      if (!add_row(root, row, cols)) {
        Wt::WStandardItem *item = root->child(row, 0);
        string name(boost::any_cast<string>(item->data(Wt::DisplayRole)));
        if (name != cols[0]) {
          item = find_row(root, cols[0], row + 1);
          if (item) {
            update_row(root, item->row(), cols);
            root->removeRows(row, item->row() - row);
          } else {
            insert_row(root, row, cols);
          }
        } else {
          update_row(root, row, cols);
        }
      }
      if (boost::starts_with(cols[0], "analysis")) {
        parent = root->child(row, 0);
        rowp = 0;
      }
      ++row;
    }
  }
  if (root->rowCount() > row) {
    root->removeRows(row, root->rowCount() - row);
  }
}


Wt::WTreeView* hs::utilization::create_view()
{
  load_stats();

  m_filter = new Wt::WSortFilterProxyModel();
  m_filter->setSourceModel(&m_stats);
  m_filter->setDynamicSortFilter(true);
  m_filter->setFilterKeyColumn(0);
  m_filter->setFilterRole(Wt::DisplayRole);

  m_view = new Wt::WTreeView();
  m_view->setSortingEnabled(true);
  m_view->resize(Wt::WLength::Auto, Wt::WLength::Auto);
  m_view->setAlternatingRowColors(true);
  m_view->setModel(m_filter);
  m_view->setColumnWidth(0, 400);
  m_view->setColumnWidth(1, 100);
  m_view->setColumnWidth(2, 75);
  m_view->setColumnWidth(3, 75);
  m_view->setColumnWidth(4, 75);
  m_view->setColumnWidth(5, 75);

  m_timer.setInterval(6000);
  m_timer.timeout().connect(this, &hs::utilization::load_stats);
  m_timer.start();
  return m_view;
}
