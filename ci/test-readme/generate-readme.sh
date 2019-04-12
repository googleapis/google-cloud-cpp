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

readonly BINDIR=$(dirname "$0")

badge() {
  local -r distro="$1"
  cat <<_EOF_

[![Kokoro install ${distro} status][kokoro-install-${distro}-shield]][kokoro-install-${distro}-link]

[kokoro-install-${distro}-shield]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-${distro}.svg
[kokoro-install-${distro}-link]: https://storage.googleapis.com/cloud-cpp-kokoro-status/kokoro-install-${distro}-link.html
_EOF_
}

echo "### CentOS (7)"
badge centos
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.centos"

echo "### Debian (Stretch)"
badge debian
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.debian"

echo "### Fedora (29)"
badge fedora
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.fedora"

echo "### OpenSUSE (Tumbleweed)"
badge opensuse
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.opensuse"

echo "### OpenSUSE (Leap)"
badge opensuse-leap
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.opensuse-leap"

echo "### Ubuntu (18.04 - Bionic Beaver)"
badge ubuntu
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.ubuntu"

echo "### Ubuntu (16.04 - Xenial Xerus)"
badge ubuntu-xenial
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.ubuntu-xenial"

echo "### Ubuntu (14.04 - Trusty Tahr)"
badge ubuntu-trusty
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.ubuntu-trusty"
