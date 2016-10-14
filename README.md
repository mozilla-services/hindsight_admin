Prerequisites {#mainpage}
====
* Clang 3.1 or GCC 4.7+
* CMake (3.5+) - http://cmake.org/cmake/resources/software.html
* Boost (1.53+) - http://www.boost.org/users/download/
* Wt (3.3.6) - http://www.webtoolkit.eu/wt/download
* sqlite (3.11+) - https://sqlite.org/

Runtime requirements
====
* source-highlight (3.1+) - https://www.gnu.org/software/src-highlite/source-highlight.html


hindsight_admin - Linux Build Instructions
====
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
