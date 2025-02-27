#!/usr/bin/env bash
# Copyright 2022 Google LLC
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

set -eo pipefail
BOLD_ON_GREY=$(tput bold && tput setab 252)
TURN_OFF_ATTR=$(tput sgr0)
SEPARATOR=

function banner() {
  test -n "${SEPARATOR}" && echo
  echo "${BOLD_ON_GREY}[ $* ]${TURN_OFF_ATTR}"
  SEPARATOR=Y
}

banner "Determining googleapis HEAD commit and tarball checksum"
REPO="googleapis/googleapis"
BRANCH="master"
if [[ -z "${COMMIT}" ]]; then
  COMMIT=$(curl -fsSL -H "Accept: application/vnd.github.VERSION.sha" \
    "https://api.github.com/repos/${REPO}/commits/${BRANCH}")
fi

if [[ -z "$COMMIT_DATE" ]]; then
  COMMIT_DATE=$(date +%Y-%m-%d)
fi

DOWNLOAD="$(mktemp)"
curl -fsSL "https://github.com/${REPO}/archive/${COMMIT}.tar.gz" -o "${DOWNLOAD}"
gsutil -q cp "${DOWNLOAD}" "gs://cloud-cpp-community-archive/com_google_googleapis/${COMMIT}.tar.gz"
SHA256=$(sha256sum "${DOWNLOAD}" | sed "s/ .*//")
SHA256_BASE64=$(openssl dgst -sha256 -binary <"${DOWNLOAD}" | openssl base64 -A)
PIPERORIGIN_REVID=
REV_COMMIT="${COMMIT}"
until grep -q "/googleapis/archive/${REV_COMMIT}\.tar" bazel/workspace0.bzl; do
  curl -fsSL -H "Accept: application/json" "https://api.github.com/repos/${REPO}/commits/${REV_COMMIT}" -o "${DOWNLOAD}"
  PIPERORIGIN_REVID=$(jq --raw-output .commit.message <"${DOWNLOAD}" |
    grep PiperOrigin-RevId:) && break
  REV_COMMIT=$(jq --raw-output '.parents[0].sha' <"${DOWNLOAD}")
done
rm -f "${DOWNLOAD}"

banner "Updating Cache for Bazel"
#bazel/deps-cache.py -p

banner "Updating Bazel/CMake dependencies"
sed -i -f - bazel/workspace0.bzl <<EOT
  /name = "com_google_googleapis",/,/strip_prefix = "/ {
    s;/com_google_googleapis/.*.tar.gz",;/com_google_googleapis/${COMMIT}.tar.gz",;
    s;/${REPO}/archive/.*.tar.gz",;/${REPO}/archive/${COMMIT}.tar.gz",;
    s/sha256 = ".*",/sha256 = "${SHA256}",/
    s/strip_prefix = "googleapis-.*",/strip_prefix = "googleapis-${COMMIT}",/
  }
EOT
sed -i -f - MODULE.bazel <<EOT
  /module_name = "googleapis",/,/^)/ {
    s;/${REPO}/archive/.*.tar.gz",;/${REPO}/archive/${COMMIT}.tar.gz",;
    s;integrity = "sha256-.*",;integrity = "sha256-${SHA256_BASE64}",;
    s/strip_prefix = "googleapis-.*",/strip_prefix = "googleapis-${COMMIT}",/
  }
EOT
sed -i -f - cmake/GoogleapisConfig.cmake <<EOT
  /^set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA$/ {
    n
    s/".*"/"${COMMIT}"/
  }
  /^set(_GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256$/ {
    n
    s/".*"/"${SHA256}"/
  }
EOT

if git diff --quiet bazel/workspace0.bzl \
  cmake/GoogleapisConfig.cmake MODULE.bazel; then
  echo "No updates"
  exit 0
fi

banner "Regenerating libraries"
# generate-libraries fails if it creates a diff, so ignore its status.
TRIGGER_TYPE='pr' ci/cloudbuild/build.sh \
  --docker --trigger=generate-libraries-pr || true

banner "Creating commits"
git commit -m"chore: update googleapis SHA circa ${COMMIT_DATE}" \
  ${PIPERORIGIN_REVID:+-m "${PIPERORIGIN_REVID}"} \
  bazel/workspace0.bzl cmake/GoogleapisConfig.cmake MODULE.bazel
if ! git diff --quiet external/googleapis/protodeps \
  external/googleapis/protolists; then
  git commit -m"Update the protodeps/protolists" \
    external/googleapis/protodeps external/googleapis/protolists
fi
if ! git diff --quiet .; then
  git commit -m"Regenerate libraries" .
fi

banner "Showing git state"
git status --untracked-files=no
echo ""
git show-branch
