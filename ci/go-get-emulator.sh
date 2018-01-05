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

set -u

if grep -q 14.04 /etc/lsb-release 2>/dev/null; then
  echo "Skipping installation of Cloud Bigtable Emulator and Cloud Bigtable" \
    "command-line client.  Go version too old in Ubuntu 14.04."
  exit 0
fi

# Make several attempts to get the Cloud Bigtable Emulator and command-line
# client, sleep longer after each failure.
min_wait=10
for i in 1 2 3 4 5; do
  echo "Fetching the Cloud Bigtable Emulator and the Cloud Bigtable" \
    "command-line client [attempt ${i}]"
  go get cloud.google.com/go/bigtable/cmd/emulator \
         cloud.google.com/go/bigtable/cmd/cbt
  if [ $? -eq 0 ]; then
    exit 0
  fi
  # Sleep for a few seconds before trying again.
  readonly PERIOD=$[ (${RANDOM} % 10) + min_wait ]
  echo "Fetch failed trying again in ${PERIOD} seconds"
  sleep ${PERIOD}s
  min_wait=$[ min_wait * 2 ]
done
