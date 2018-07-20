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

if [ "${BUILD_TYPE:-Release}" != "Coverage" ]; then
    echo "Skipping code coverage as it is disabled for this build."
    exit 0
fi

# Upload the results using the script from codecov.io
# Save the log to a file because it exceeds the 4MB limit in Travis.
readonly CI_ENV=$(bash <(curl -s https://codecov.io/env))
readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
sudo docker run $CI_ENV \
    --volume $PWD:/v --workdir /v \
    "${IMAGE}:tip" /bin/bash -c "/bin/bash <(curl -s https://codecov.io/bash)"

echo "================================================================"
head -1000 codecov.log
echo "================================================================"
tail -1000 codecov.log
echo "================================================================"
