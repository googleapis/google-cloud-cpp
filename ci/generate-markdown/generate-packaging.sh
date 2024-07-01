#!/usr/bin/env bash
#
# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

BINDIR=$(dirname "$0")
readonly BINDIR

file="doc/packaging.md"

(
  sed '/<!-- inject-distro-instructions-start -->/q' "${file}"

  # Extracts the part of a file between the BEGIN/DONE tags.
  function extract() {
    sed -e '0,/^.*\[BEGIN packaging.md\].*$/d' \
      -e '/^.*\[DONE packaging.md\].*$/,$d' "$1" | sed -e '/testing_details/d'
  }

  # A "map" (comma separated) of dockerfile -> summary.
  DOCKER_DISTROS=(
    "demo-alpine-stable.Dockerfile,Alpine (Stable)"
    "demo-fedora.Dockerfile,Fedora (40)"
    "demo-opensuse-leap.Dockerfile,openSUSE (Leap)"
    "demo-ubuntu-24.04.Dockerfile,Ubuntu (24.04 LTS - Noble Numbat)"
    "demo-ubuntu-jammy.Dockerfile,Ubuntu (22.04 LTS - Jammy Jellyfish)"
    "demo-ubuntu-focal.Dockerfile,Ubuntu (20.04 LTS - Focal Fossa)"
    "demo-debian-bookworm.Dockerfile,Debian (12 - Bookworm)"
    "demo-debian-bullseye.Dockerfile,Debian (11 - Bullseye)"
    "demo-rockylinux-9.Dockerfile,Rocky Linux (9)"
  )
  for distro in "${DOCKER_DISTROS[@]}"; do
    dockerfile="$(cut -f1 -d, <<<"${distro}")"
    summary="$(cut -f2- -d, <<<"${distro}")"
    echo
    echo "<details>"
    echo "<summary>${summary}</summary>"
    echo "<br>"
    extract "${BINDIR}/../cloudbuild/dockerfiles/${dockerfile}" |
      "${BINDIR}/dockerfile2markdown.sh"
    echo "#### Compile and install the main project"
    echo
    echo "We can now compile and install \`google-cloud-cpp\`:"
    echo
    echo '```bash'
    extract "${BINDIR}/../cloudbuild/builds/demo-install.sh"
    echo '```'
    echo
    echo "</details>"
  done
  echo
  sed -n '/<!-- inject-distro-instructions-end -->/,$p' "${file}"
) | sponge "${file}"
