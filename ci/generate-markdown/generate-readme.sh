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

(
  sed '/<!-- inject-quickstart-start -->/q' "README.md"
  echo '```cc'
  # Dumps the contents of quickstart.cc starting at the first #include, so we
  # skip the license header comment.
  sed -n -e '/END .*quickstart/,$d' -e '/^#/,$p' "google/cloud/storage/quickstart/quickstart.cc"
  echo '```'
  sed -n '/<!-- inject-quickstart-end -->/,$p' "README.md"
) >"README.md.tmp"
mv "README.md.tmp" "README.md"

(
  mapfile -t libraries < <(bazelisk --batch query \
    --noshow_progress --noshow_loading_progress \
    'kind(cc_library, //:all) except filter("experimental|mocks", kind(cc_library, //:all))' |
    sed -e 's;//:;;' |
    LC_ALL=C sort)
  sed '/<!-- inject-GA-libraries-start -->/q' "README.md"
  for library in "${libraries[@]}"; do
    description="$(sed -n '1 s/# \(.*\) C++ Client Library/\1/p' "google/cloud/${library}/README.md")"
    printf '* [%s](google/cloud/%s/README.md) [[quickstart]](google/cloud/%s/quickstart/README.md)\n' \
      "${description}" "${library}" "${library}"
  done
  sed -n '/<!-- inject-GA-libraries-end -->/,$p' "README.md"
) >"README.md.tmp"
mv "README.md.tmp" "README.md"
