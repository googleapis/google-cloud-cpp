#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

if [ "${BUILD_TYPE:-Release}" != "Coverage" ]; then
    echo "Skipping code coverage as it is disabled for this build."
    exit 0
fi

# Upload the results using the script from codecov.io
# Save the log to a file because it exceeds the 4MB limit in Travis.
echo -n "Uploading code coverage to codecov.io..."
readonly CI_ENV=$(bash <(curl -s https://codecov.io/env))
sudo docker run $CI_ENV \
    --volume $PWD:/v --workdir /v \
    "${IMAGE}:tip" /bin/bash -c \
    "/bin/bash <(curl -s https://codecov.io/bash) -g './build-output/ccache/*' >/v/codecov.log 2>&1"
echo "DONE"

dump_log codecov.log
