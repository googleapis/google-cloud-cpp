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

readonly BINDIR=$(dirname $0)

echo "### CentOS (7)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.centos"

echo "### Debian (Stretch)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.debian"

echo "### Fedora (29)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.fedora"

echo "### OpenSUSE (Tumbleweed)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.opensuse"

echo "### OpenSUSE (Leap)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.opensuse-leap"

echo "### Ubuntu (18.04 - Bionic Beaver)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.ubuntu"

echo "### Ubuntu (16.04 - Xenial Xerus)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.ubuntu-xenial"

echo "### Ubuntu (14.04 - Trusty Tahr)"
"${BINDIR}/extract-readme.sh" "${BINDIR}/Dockerfile.ubuntu-trusty"
