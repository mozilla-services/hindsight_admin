/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// @brief Hindsight Source Viewer @file

#ifndef hindsight_admin_source_viewer_h_
#define hindsight_admin_source_viewer_h_

#include <string>

#include <Wt/WViewWidget>
#include <Wt/WWidget>

namespace mozilla {
namespace services {
namespace hindsight {

class source_viewer : public Wt::WViewWidget {
public:
  source_viewer() { }
  void set_filename(const std::string &fn);
  const std::string& get_filename() {
    return m_file;
  }

protected:
  virtual Wt::WWidget* renderView() override;

private:
  std::string m_file;
};


}
}
}
#endif

