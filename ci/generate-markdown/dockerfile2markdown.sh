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

# shellcheck disable=SC2016
sed_args=(
  # Delete any sections marked as ignored.
  '-e' '/^## \[START IGNORED\]/,/^## \[END IGNORED\]/d'
  # Remove the Docker commands to define the base image and/or copy files.
  '-e' '/^FROM /d'
  '-e' '/^ARG /d'
  '-e' '/^COPY /d'
  # Things that are double comments are removed. This allows us to insert
  # comments in the Dockerfile that are not transferred to the image.
  '-e' '/^## /d'
  # Regular comments are converted to text, this is where we put most of the
  # markdown and explanations we intend to output to the INSTALL or README
  # file
  '-e' 's/^# //'
  # Regular Dockerfile commands just become commands.
  '-e' 's/^RUN //'
  # Change some Dockerfile commands to the shell counterparts
  '-e' 's/^WORKDIR /cd /'
  '-e' 's/^ENV /export /'
  # To workaround transient download failures the CI builds execute some
  # commands 3 times, we do not need that in the README or INSTALL
  # instructions
  '-e' 's,/retry3 ,,'
  # A number of commands need to be executed as sudo when the user is running
  # them. The Dockerfile runs as root, and does not need the sudo prefix, but
  # also, sudo may not even be installed.
  '-e' 's/update-alternatives/sudo update-alternatives/g'
  '-e' 's/add-apt-repository/sudo add-apt-repository/g'
  '-e' 's/apt /sudo apt /g'
  '-e' 's/apt-get /sudo apt-get /g'
  '-e' 's/dnf /sudo dnf /g'
  '-e' 's/rpm /sudo rpm /g'
  '-e' 's/yum /sudo yum /g'
  '-e' 's/yum-config-manager /sudo yum-config-manager /g'
  '-e' 's/zypper /sudo zypper /g'
  '-e' 's/ldconfig/sudo ldconfig/g'
  # Any 'make install' or 'cmake ... --target install' commands need sudo.
  '-e' 's/\(cmake.*--target install.*\)/sudo \1/g'
  '-e' 's/make install/sudo make install/'
  # A standard technique to write files with root permissions is tee.
  '-e' 's,tee /,sudo tee /,g'
  # In some cases we link paths in /usr/bin, this is a bit too generous of a
  # match, but works in practice.
  '-e' 's/ln -sf /sudo ln -sf /g'
  # The Dockerfile can use absolute paths for some directories, in the README
  # or INSTALL instructions we prefer to use $HOME
  '-e' 's;/home/build;$HOME;'
  '-e' 's;/var/tmp/build;$HOME/Downloads;'
  # Remove additional indentation for the sudo commands. The Docker files
  # are more readable with the indentation, the markdown files not so much.
  '-e' 's/^    sudo/sudo/'
)

sed "${sed_args[@]}" "$@" |
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
