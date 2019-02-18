#!/usr/bin/env bash
#
# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eu

sed \
    -e '/^## \[START IGNORED\]/,/^## \[END IGNORED\]/d' \
    -e '/^## /d' \
    -e 's/^# //' \
    -e 's/^WORKDIR /cd /' \
    -e 's/^RUN //' \
    -e 's/^ENV /export /' \
    -e '/^COPY /d' \
    -e 's/update-alternatives/sudo update-alternatives/g' \
    -e 's/add-apt-repository/sudo add-apt-repository/g' \
    -e 's/apt /sudo apt /g' \
    -e 's/dnf/sudo dnf/g' \
    -e 's/yum/sudo yum/g' \
    -e 's/zypper/sudo zypper/g' \
    -e 's/ldconfig/sudo ldconfig/g' \
    -e 's/^\(cmake.*--target install.*\)/sudo \1/g' \
    -e 's/^make install/sudo make install/' \
    -e 's;/home/build;$HOME;' \
    -e 's;/var/tmp/build;$HOME/Downloads;' \
    -e 's/^    sudo/sudo/' "$@" | \
    awk '
      BEGIN {
        removing_blanks=0;
      }
      {
        if ($0 != "") {
          removing_blanks = 0;
          print $0;
        } else if (removing_blanks != 1) {
          removing_blanks = 1;
          print $0;
        }
      }
    '
