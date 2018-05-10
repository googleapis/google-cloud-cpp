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

delay=1
connected=no
readonly ATTEMPTS=$(seq 1 8)
for attempt in $ATTEMPTS; do
  if curl "https://nghttp2.org/httpbin/get" >/dev/null 2>&1; then
    connected=yes
    break
  fi
  sleep $delay
  delay=$((delay * 2))
done

if [ "${connected}" = "no" ]; then
  # TODO(..) - use a local server for the integration tests.
  echo "Cannot connect to nghttp2.org; exit test with success." >&2
  exit 0
else
  echo "Successfully connected to nghttp2.org."
fi

echo
echo "Running storage::internal::CurlRequest integration test."
./curl_request_integration_test

exit 0
