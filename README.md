# Hindsight Administration User Interface

## Overview

Prototype administration user interface for Hindsight. Live demo:
https://hsadmin.trink.com/

## Build

### Prerequisites

* GCC (4.8+)
* CMake (3.5+) - http://cmake.org/cmake/resources/software.html
* Boost (1.65.1) - http://www.boost.org/users/download/
* Wt (3.3.9) - http://www.webtoolkit.eu/wt/download
* sqlite (3.11+) - https://sqlite.org/

### Runtime requirements

* source-highlight (3.1+) - https://www.gnu.org/software/src-highlite/source-highlight.html

### CMake Build Instructions (Linux only)

    git clone https://github.com/mozilla-services/hindsight_admin.git
    cd hindsight_admin
    mkdir release
    cd release
    cmake -DCMAKE_BUILD_TYPE=release ..
    make

    # to run the web server locally
    # update the hindsight_admin_server.xml with your configuration keys
    cpack -G DEB
    sudo dpkg -i hindsight-admin-0.0.1-Linux.deb
    /usr/share/hindsight_admin/run.sh
    # http://localhost:2020/

## Releases

* The master branch is the current release and is considered stable at all
  times.
* New versions can be released as frequently as every two weeks (our sprint
  cycle). The only exception would be for a high priority patch.
* All active work is flagged with the sprint milestone and tracked in the
  project dashboard.
* New releases occur the day after the sprint finishes.
  * The version in the dev branch is updated
  * The changes are merged into master
  * A new tag is created

## Contributions

* All pull requests must be made against the dev branch, direct commits to
  master are not permitted.
* All non trivial contributions should start with an issue being filed (if it is
  a new feature please propose your design/approach before doing any work as not
  all feature requests are accepted).
