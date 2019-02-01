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


if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

echo "================================================================"
printenv
echo "================================================================"

echo "================================================================"
echo "Updating submodules."
cd "${PROJECT_ROOT}"
git submodule update --init
echo "================================================================"

echo "================================================================"
echo "Creating Docker image with all the development tools."
set +e
"${PROJECT_ROOT}/ci/travis/install-linux.sh" \
    >create-build-docker-image.log 2>&1 </dev/null
if [[ "$?" != 0 ]]; then
  dump_log create-build-docker-image.log
  exit 1
fi
echo "================================================================"

set -e
echo "================================================================"
echo "Running the full build."

export NEEDS_CCACHE=no
"${PROJECT_ROOT}/ci/travis/build-linux.sh"
echo "================================================================"

echo "================================================================"
"${PROJECT_ROOT}/ci/travis/dump-logs.sh"
echo "================================================================"
