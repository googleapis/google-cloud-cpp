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

cat <<'END_OF_PREAMBLE'
# Installing google-cloud-cpp

By default google-cloud-cpp libraries download and compile all their
dependencies ([see below](#required-libraries) for a complete list). This makes
it easier for users to "take the library for a spin", and works well for users
that "Live at Head", but does not work for package maintainers or users that
prefer to compile their dependencies once and install them in `/usr/local/` or a
similar directory.

This document provides instructions to install the dependencies of
google-cloud-cpp.

**If** all the dependencies of `google-cloud-cpp` are installed and provide
CMake support files, then compiling and installing the libraries
requires two commands:

```bash
cmake -H. -Bbuild-output-for-install \
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package
cmake --build build-output-for-install --target install
```

Unfortunately getting your system to this state may require multiple steps,
the following sections describe how to install `google-cloud-cpp` on several
platforms.

### Required Libraries

`google-cloud-cpp` directly depends on the following libraries:

| Library | Minimum version | Description |
| ------- | --------------: | ----------- |
| gRPC    | 1.17.x | gRPC++ for Cloud Bigtable |
| libcurl | 7.47.0  | HTTP client library for the Google Cloud Storage client |
| crc32c  | 1.0.6 | Hardware-accelerated CRC32C implementation |
| OpenSSL | 1.0.2 | Crypto functions for Google Cloud Storage authentication |

Note that these libraries may also depend on other libraries. The following
instructions include steps to install these indirect dependencies too.

When possible, the instructions below prefer to use pre-packaged versions of
these libraries and their dependencies. In some cases the packages do not exist,
or the package versions are too old to support `google-cloud-cpp`. If this is
the case, the instructions describe how you can manually download and install
these dependencies.

## Table of Contents

- [Fedora 29](#fedora-29)
- [openSUSE (Tumbleweed)](#opensuse-tumbleweed)
- [openSUSE (Leap)](#opensuse-leap)
- [Ubuntu (18.04 - Bionic Beaver)](#ubuntu-1804---bionic-beaver)
- [Ubuntu (16.04 - Xenial Xerus)](#ubuntu-1604---xenial-xerus)
- [Ubuntu (16.04 - Trusty Tahr)](#ubuntu-1404---trusty-tahr)
- [Debian (Stretch)](#debian-stretch)
- [CentOS 7](#centos-7)
END_OF_PREAMBLE

echo
echo "### Fedora (29)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.fedora"

echo
echo "### OpenSUSE (Tumbleweed)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.opensuse"

echo
echo "### OpenSUSE (Leap)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.opensuse-leap"

echo
echo "### Ubuntu (18.04 - Bionic Beaver)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.ubuntu"

echo
echo "### Ubuntu (16.04 - Xenial Xerus)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.ubuntu-xenial"

echo
echo "### Ubuntu (14.04 - Trusty Tahr)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.ubuntu-trusty"

echo
echo "### Debian (Stretch)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.debian"

echo
echo "### CentOS (7)"
"${BINDIR}/extract-install.sh" "${BINDIR}/Dockerfile.centos"
