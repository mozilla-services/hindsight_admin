/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Plugins Implementation @file

#include "plugins.h"

#include <fstream>
#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <Wt/WContainerWidget>
#include <Wt/WLink>
#include <Wt/WMenuItem>
#include <Wt/WMessageBox>
#include <Wt/WPopupMenu>
#include <Wt/WPopupMenuItem>
#include <Wt/WPushButton>
#include <Wt/WScrollArea>
#include <Wt/WStandardItem>
#include <Wt/WText>
#include <Wt/WVBoxLayout>
#include <Wt/WViewWidget>

#include "constants.h"

using namespace std;
namespace hs = mozilla::services::hindsight;
namespace fs = boost::filesystem;

hs::plugins::plugins(hs::session *s, const hindsight_cfg *cfg) :
    m_session(s),
    m_hs_cfg(cfg),
    m_stats(0, stat_columns + 2),
    m_popup(nullptr)
{
  m_stats.setHeaderData(0, Wt::Horizontal, tr("state"));
  m_stats.setHeaderData(1, Wt::Horizontal, tr("type"));
  m_stats.setHeaderData(2, Wt::Horizontal, tr("plugin_name"));
  m_stats.setHeaderData(3, Wt::Horizontal, tr("im_count"));
  m_stats.setHeaderData(4, Wt::Horizontal, tr("im_bytes"));
  m_stats.setHeaderData(5, Wt::Horizontal, tr("pm_count"));
  m_stats.setHeaderData(6, Wt::Horizontal, tr("pm_failures"));
  m_stats.setHeaderData(7, Wt::Horizontal, tr("cur_mem"));
  m_stats.setHeaderData(8, Wt::Horizontal, tr("max_mem"));
  m_stats.setHeaderData(9, Wt::Horizontal, tr("max_output"));
  m_stats.setHeaderData(10, Wt::Horizontal, tr("max_inst"));
  m_stats.setHeaderData(11, Wt::Horizontal, tr("mm_avg"));
  m_stats.setHeaderData(12, Wt::Horizontal, tr("mm_sd"));
  m_stats.setHeaderData(13, Wt::Horizontal, tr("pm_avg"));
  m_stats.setHeaderData(14, Wt::Horizontal, tr("pm_sd"));
  m_stats.setHeaderData(15, Wt::Horizontal, tr("te_avg"));
  m_stats.setHeaderData(16, Wt::Horizontal, tr("te_sd"));
  this->addWidget(create_view());
}


void hs::plugins::load_run_path(const fs::path &dir)
{
  fs::path run_path(m_hs_cfg->m_hs_run / dir);
  boost::smatch matches;
  for (fs::directory_iterator dit(run_path); dit != fs::directory_iterator(); ++dit) {
    string fn(dit->path().filename().string());
    if (boost::regex_match(fn, matches, boost::regex("(.+)\\.(off|cfg)$"))) {
      vector<Wt::WStandardItem *> items;
      Wt::WStandardItem *item = new Wt::WStandardItem();
      if (string(matches[2].first, matches[2].second) == "off") {
        item->setData(tr("stopped"), Wt::DisplayRole);
        item->setData(LSB_STOP, Wt::UserRole);
      } else {
        item->setData(tr("unknown"), Wt::DisplayRole);
        item->setData(LSB_UNKNOWN, Wt::UserRole);
      }
      items.push_back(item);

      item = new Wt::WStandardItem();
      item->setData(dir.string(), Wt::DisplayRole);
      items.push_back(item);

      item = new Wt::WStandardItem();
      item->setData(string(matches[1].first, matches[1].second), Wt::DisplayRole);
      items.push_back(item);

      for (int col = 1; col < stat_columns; ++col) {
        item = new Wt::WStandardItem();
        item->setData(0.0, Wt::DisplayRole);
        items.push_back(item);
      }
      m_stats.appendRow(items);
    }
  }
}


int hs::plugins::add_new_row(const std::string &ptype,
                             const std::string &pname,
                             lsb_state state)
{
  vector<Wt::WStandardItem *> items;
  Wt::WStandardItem *item = new Wt::WStandardItem();
  item->setData(state, Wt::UserRole);
  if (state == LSB_RUNNING) {
    item->setData(tr("running"), Wt::DisplayRole);
    item->setStyleClass("running");
  } else if (state == LSB_UNKNOWN - 1) {
    item->setData(tr("deploying"), Wt::DisplayRole);
    item->setStyleClass("deploying");
  } else {
    item->setData(tr("unknown"), Wt::DisplayRole);
    item->setStyleClass("unknown");
  }
  items.push_back(item);

  item = new Wt::WStandardItem();
  item->setData(ptype, Wt::DisplayRole);
  items.push_back(item);

  item = new Wt::WStandardItem();
  item->setData(pname, Wt::DisplayRole);
  items.push_back(item);

  for (int col = 1; col < stat_columns; ++col) {
    item = new Wt::WStandardItem();
    item->setData(0.0, Wt::DisplayRole);
    items.push_back(item);
  }
  m_stats.appendRow(items);
  return m_stats.rowCount() - 1;
}


int hs::plugins::find_create(const std::string plugin, lsb_state state)
{
  size_t pos   = plugin.find_first_of(".");
  string ptype = plugin.substr(0, pos);
  string pname = plugin.substr(pos + 1);

  int rows = m_stats.rowCount();
  for (int row = 0; row < rows; ++row) {
    string ptype1(boost::any_cast<string>(m_stats.item(row, 1)->data(Wt::DisplayRole)));
    string pname1(boost::any_cast<string>(m_stats.item(row, 2)->data(Wt::DisplayRole)));
    if (ptype == ptype1 && pname == pname1) {
      m_stats.item(row)->setData(LSB_RUNNING, Wt::UserRole);
      m_stats.item(row)->setData(tr("running"), Wt::DisplayRole);
      m_stats.item(row)->setStyleClass("running");
      return row;
    }
  }
  return add_new_row(ptype, pname, state);
}



void hs::plugins::load_stats()
{
  if (m_hs_cfg->m_plugins.empty()) return;

  bool header = true;
  int cnt     = 0;
  string s;
  ifstream ifs(m_hs_cfg->m_plugins.string().c_str());
  vector<string> cols;

  int rows = m_stats.rowCount();
  for (int i = 0; i < rows; ++i) {
    int state = boost::any_cast<lsb_state>(m_stats.item(i)->data());
    if (state == LSB_RUNNING) {
      m_stats.item(i)->setData(LSB_UNKNOWN, Wt::UserRole); // state flag
    }
  }

  while (getline(ifs, s).good()) {
    boost::split(cols, s, boost::is_any_of("\t"));
    cnt = cols.size();
    if (cnt < stat_columns) continue;

    if (header) {
      header = false;
      continue;
    }

    int row = find_create(cols[0], LSB_RUNNING);
    for (int col = 1; col < stat_columns; ++col) {
      m_stats.item(row, col + 2)->setData(boost::lexical_cast<double>(cols[col]),
                                          Wt::DisplayRole);
    }
  }

  rows = m_stats.rowCount();
  for (int i = 0; i < rows; ++i) {
    Wt::WStandardItem *item = m_stats.item(i);
    int state = boost::any_cast<lsb_state>(item->data());
    if (state != LSB_RUNNING) {
      string ptype(boost::any_cast<string>(m_stats.item(i, 1)->data(Wt::DisplayRole)));
      string pname(boost::any_cast<string>(m_stats.item(i, 2)->data(Wt::DisplayRole)));
      if (fs::exists(m_hs_cfg->m_hs_run / ptype / (pname + ".err"))) {
        item->setData(tr("terminated"), Wt::DisplayRole);
        item->setData(LSB_TERMINATED, Wt::UserRole);
        item->setStyleClass("terminated");
      } else if (fs::exists(m_hs_cfg->m_hs_run / ptype / (pname + ".off"))) {
        item->setData(tr("stopped"), Wt::DisplayRole);
        item->setData(LSB_STOP, Wt::UserRole);
        item->setStyleClass("stopped");
      } else if (!fs::exists(m_hs_cfg->m_hs_load / ptype / (pname + ".cfg")) &&
                 !fs::exists(m_hs_cfg->m_hs_run / ptype / (pname + ".cfg"))) {
        auto items = m_stats.takeRow(i);
        for (auto ci = items.begin(); ci != items.end(); ++ci) {
          delete(*ci);
        }
        --i;
        --rows;
      }
    }
  }
}


Wt::WTableView* hs::plugins::create_view()
{
  load_run_path("input");
  load_run_path("analysis");
  load_run_path("output");
  load_stats();

  m_filter = new Wt::WSortFilterProxyModel();
  m_filter->setSourceModel(&m_stats);
  m_filter->setDynamicSortFilter(true);
  m_filter->setFilterKeyColumn(2);
  m_filter->setFilterRole(Wt::DisplayRole);

  m_view = new Wt::WTableView();
  m_view->setSortingEnabled(true);
  m_view->resize(Wt::WLength::Auto, Wt::WLength::Auto);
  m_view->setAlternatingRowColors(true);
  m_view->setModel(m_filter);
  m_view->setColumnWidth(0, 70);
  m_view->setSortingEnabled(0, false); // todo fix PKc error
  m_view->setColumnWidth(1, 55);
  m_view->setColumnWidth(2, 150);
  for (int i = 3; i < stat_columns + 2; ++i) {
    m_view->setColumnWidth(i, 80);
  }
  m_view->setSelectionMode(Wt::SingleSelection);
  /*
   * To support right-click, we need to disable the built-in browser
   * context menu.
   *
   * Note that disabling the context menu and catching the
   * right-click does not work reliably on all browsers.
   */
  m_view->setAttributeValue("oncontextmenu", "event.cancelBubble = true; event.returnValue = false; return false;");
  m_view->mouseWentUp().connect(this, &hs::plugins::popup);

  m_timer.setInterval(6000);
  m_timer.timeout().connect(this, &hs::plugins::load_stats);
  m_timer.start();
  return m_view;
}


void hs::plugins::popup(const Wt::WModelIndex &item, const Wt::WMouseEvent &event)
{
  if (event.button() == Wt::WMouseEvent::RightButton) {
    if (!m_view->isSelected(item)) m_view->select(item);

    if (m_popup) {
      delete m_popup;
      m_popup = NULL;
    }
    int row = m_filter->mapToSource(item).row();
    int state = boost::any_cast<lsb_state>(m_stats.data(row, 0, Wt::UserRole));
    if (state <= 0) {
      return;
    }

    string plugin(boost::any_cast<string>(m_stats.data(row, 2, Wt::DisplayRole)));
    m_popup = new Wt::WPopupMenu();
    m_popup->setAutoHide(true, 10);
    m_popup->addItem(tr("view_lua"))->setData(reinterpret_cast<void *>(lua));

    if (state == LSB_TERMINATED) {
      m_popup->addItem(tr("view_termination_error"))->setData(reinterpret_cast<void *>(err));
    }

    if (plugin.find(m_session->user_name() + ".") != string::npos) {
      m_popup->addItem(tr("edit_plugin"))->setData(reinterpret_cast<void *>(edit));
      switch (state) {
      case LSB_RUNNING:
        m_popup->addSeparator();
        m_popup->addItem(tr("stop_plugin"))->setData(reinterpret_cast<void *>(stop));
        break;
      case LSB_STOP:
        m_popup->addSeparator();
        m_popup->addItem(tr("restart_plugin"))->setData(reinterpret_cast<void *>(restart));
        m_popup->addItem(tr("delete_plugin"))->setData(reinterpret_cast<void *>(del));
        break;
      case LSB_TERMINATED:
        m_popup->addSeparator();
        m_popup->addItem(tr("delete_plugin"))->setData(reinterpret_cast<void *>(del));
        break;
      default:
        break;
      }
    } else {
      m_popup->addItem(tr("copy_plugin"))->setData(reinterpret_cast<void *>(edit));
    }

    m_popup->aboutToHide().connect(this, &hs::plugins::popup_action);
    if (m_popup->isHidden()) {
      m_popup->popup(event);
    } else {
      m_popup->hide();
    }
  }
}


void hs::plugins::message_box_dismissed()
{
  delete m_message_box;
  m_message_box = NULL;
}


void hs::plugins::popup_action()
{
  if (m_popup->result()) {
    m_popup->hide();
    if (m_view->selectedIndexes().empty()) {
      return;
    }
    Wt::WModelIndex selected = *m_view->selectedIndexes().begin();
    int row = selected.row();
    Wt::WModelIndex tidx = selected.model()->index(row, 1);
    Wt::WModelIndex index = selected.model()->index(row, 2);
    boost::any d = index.data(Wt::DisplayRole);
    if (d.empty()) {
      return;
    }
    string plugin(boost::any_cast<string>(d));
    string ptype(boost::any_cast<string>(tidx.data(Wt::DisplayRole)));

    intptr_t data = reinterpret_cast<intptr_t>(m_popup->result()->data()); // the pointer value is the data
    popup_actions action = static_cast<popup_actions>(data);
    switch (action) {
//    case user_filter:
//      {
//        Wt::WModelIndex selected = *m_view->selectedIndexes().begin();
//        Wt::WModelIndex index = selected.model()->index(selected.row(), 2);
//        boost::any d = index.data(Wt::DisplayRole);
//        if (!d.empty()) {
//          string plugin(boost::any_cast<std::string>(d));
//          string instance(plugin.substr(plugin.find_first_of('.') + 1));
//          m_filter->setFilterKeyColumn(2);
//          m_filter->setFilterRegExp("^" + instance);
//        }
//      }
//      break;

    case lua:
      {
        Wt::WMessageBox *mb = new Wt::WMessageBox();
        mb->setWindowTitle(plugin);
        mb->setClosable(true);
        Wt::WPanel *cfgp = new Wt::WPanel();
        mb->contents()->addWidget(cfgp);
        cfgp->setTitle(tr("configuration"));
        Wt::WPanel *luap = new Wt::WPanel();
        mb->setMaximumSize(900, 750);
        mb->contents()->addWidget(luap);

        boost::filesystem::path cfg_file = m_hs_cfg->m_hs_run / ptype / (plugin + ".rtc");
        Wt::WScrollArea *sa = new Wt::WScrollArea();
        cfgp->setCentralWidget(sa);
        cfg_viewer *cv = new cfg_viewer();
        cv->set_filename(cfg_file.string());
        sa->setWidget(cv);
        cfgp->setCollapsible(true);
        cfgp->setCollapsed(true);

        boost::filesystem::path lua_file = m_hs_cfg->m_hs_run / ptype / cv->get_lua_file();
        if (!exists(lua_file)) {
          lua_file = m_hs_cfg->m_hs_install / ptype / cv->get_lua_file();
        }
        luap->setTitle(cv->get_lua_file());
        sa = new Wt::WScrollArea();
        luap->setCentralWidget(sa);
        source_viewer *fv = new source_viewer();
        fv->set_filename(lua_file.string());
        sa->setWidget(fv);
        luap->setCollapsible(false);

        mb->setModal(false);
        mb->buttonClicked().connect(std::bind([=]()
        {
          delete mb;
        }));

        mb->show();
      }
      break;

    case stop:
      {
        Wt::WString title(tr("success"));
        Wt::WString msg;
        if (plugin.find(m_session->user_name() + ".") != string::npos) {
          Wt::StandardButton result = Wt::WMessageBox::show(tr("confirm"), tr("stop_plugin?").arg(plugin), Wt::Ok | Wt::Cancel);
          if (result != Wt::Ok) return;
          msg = tr("plugin_control").arg(plugin).arg(tr("stopped"));
          fs::path active_cfg(m_hs_cfg->m_hs_run / ptype / (plugin + ".cfg"));
          if (exists(active_cfg)) {
            fs::path disable(m_hs_cfg->m_hs_load / ptype / (plugin + ".off"));
            ofstream ofs(disable.string().c_str());
            if (!ofs) {
              msg = tr("plugin_control_failed").arg(plugin).arg(tr("stopped"));
            }
          }
        } else {
          title = tr("error");
          msg = tr("plugin_not_owner").arg(plugin).arg(tr("stopped"));
        }
        m_message_box = new Wt::WMessageBox(title, msg, Wt::NoIcon, Wt::Ok);
        m_message_box->buttonClicked().connect(this, &hs::plugins::message_box_dismissed);
        m_message_box->show();
      }
      break;

    case restart:
      {
        Wt::WString title(tr("success"));
        Wt::WString msg;
        if (plugin.find(m_session->user_name() + ".") != string::npos) {
          Wt::StandardButton result = Wt::WMessageBox::show(tr("confirm"), tr("restart_plugin?").arg(plugin), Wt::Ok | Wt::Cancel);
          if (result != Wt::Ok) return;
          msg = tr("plugin_control").arg(plugin).arg(tr("restarted"));
          fs::path disable(m_hs_cfg->m_hs_run / ptype / (plugin + ".off"));
          fs::path reload_cfg(m_hs_cfg->m_hs_load / ptype / (plugin + ".cfg"));
          try {
            copy(disable, reload_cfg);
          } catch (...) {
            msg = tr("plugin_control_failed").arg(plugin).arg(tr("restarted"));
          }
        } else {
          title = tr("error");
          msg = tr("plugin_not_owner").arg(plugin).arg(tr("restarted"));
        }
        m_message_box = new Wt::WMessageBox(title, msg, Wt::NoIcon, Wt::Ok);
        m_message_box->buttonClicked().connect(this, &hs::plugins::message_box_dismissed);
        m_message_box->show();
      }
      break;

    case edit:
      // todo
      break;

    case del:
      {
        if (plugin.find(m_session->user_name() + ".") != string::npos) {
          remove(m_hs_cfg->m_hs_run / ptype / (plugin + ".lua"));
          remove(m_hs_cfg->m_hs_run / ptype / (plugin + ".off"));
          remove(m_hs_cfg->m_hs_run / ptype / (plugin + ".cfg"));
          remove(m_hs_cfg->m_hs_run / ptype / (plugin + ".rtc"));
          remove(m_hs_cfg->m_hs_run / ptype / (plugin + ".err"));
          m_filter->removeRows(row, 1);
        } else {
          Wt::WString msg;
          msg = tr("plugin_not_owner").arg(plugin).arg(tr("deleted"));
          m_message_box = new Wt::WMessageBox(tr("error"), msg, Wt::NoIcon, Wt::Ok);
          m_message_box->buttonClicked().connect(this, &hs::plugins::message_box_dismissed);
          m_message_box->show();
        }
      }
      break;

    case err:
      {
        Wt::WString title(tr("termination_error"));
        fs::path fn = m_hs_cfg->m_hs_run / ptype / (plugin + ".err");
        ifstream t(fn.string());
        stringstream buf;
        buf << t.rdbuf();
        Wt::WString msg(buf.str());
        m_message_box = new Wt::WMessageBox(title, msg, Wt::NoIcon, Wt::Ok);
        m_message_box->setMaximumSize(800, Wt::WLength::Auto);
        m_message_box->buttonClicked().connect(this, &hs::plugins::message_box_dismissed);
        m_message_box->show();
      }
      break;

    default:
      // do nothing
      break;
    }
  } else {
    m_popup->hide();
  }
}
