#!/bin/bash
#
# Usage:
#   $ release.sh [-f] <organization/project-name> [<new-version>]
#
#   Args:
#     organization/project-name    Required. The GitHub repo to release.
#
#     new-version                  Optional. The new version number to use,
#                                  specified as M.N.0. If not specified, the
#                                  new version will be computed from existing
#                                  git tags. This flag should only be needed
#                                  when jumping to a non-sequential version
#                                  number.
#
#   Options:
#     -f     Force; actually make and push the changes
#     -h     Print help message
#
# This script creates a "release" on github by doing the following:
#
#   1. Computes the next version to use, if not specified on the command line
#   2. Creates and pushes the tag w/ the new version
#   3. Creates and pushes a new branch w/ the new version
#   4. Creates the "Pre-Release" in the GitHub UI.
#
# Before running this script the user should make sure the CHANGELOG.md on
# master is up-to-date with the release notes for the new release that will
# happen. Then run this script. After running this script, the user must still
# go to the GH UI where the new release will exist as a "pre-release", and edit
# the release notes.
#
# Examples:
#
#   # NO CHANGES ARE PUSHED. Shows what commands would be run.
#   $ release.sh googleapis/google-cloud-cpp-spanner
#
#   # NO CHANGES ARE PUSHED. Shows what commands would be run.
#   $ release.sh googleapis/google-cloud-cpp-spanner 2.0.0
#
#   # PUSHES CHANGES to release -spanner
#   $ release.sh -f googleapis/google-cloud-cpp-spanner
#
#   # PUSHES CHANGES to release -spanner, setting its new version to 2.0.0
#   $ release.sh -f googleapis/google-cloud-cpp-spanner 2.0.0

set -eu

# Extracts all the documentation at the top of this file as the usage text.
readonly USAGE="$(sed -n '3,/^$/s/^# \?//p' "$0")"

# Takes an optional list of strings to be printed with a trailing newline and
# exits the program with code 1
function die_with_message() {
  for m in "$@"; do
    echo "$m" 1>&2
  done
  exit 1
}

FORCE_FLAG="no"
while getopts "fh" opt "$@"; do
  case "$opt" in
    [f])
      FORCE_FLAG="yes";;
    [h])
      echo "${USAGE}"
      exit 0;;
    *)
      die_with_message "${USAGE}";;
  esac
done
shift $((OPTIND - 1))
declare -r FORCE_FLAG

PROJECT_ARG=""
VERSION_ARG=""
if [[ $# -eq 1 ]]; then
  PROJECT_ARG="$1"
elif [[ $# -eq 2 ]]; then
  PROJECT_ARG="$1"
  VERSION_ARG="$2"
else
  die_with_message "Invalid arguments" "${USAGE}"
fi
declare -r PROJECT_ARG
declare -r VERSION_ARG

readonly CLONE_URL="git@github.com:${PROJECT_ARG}.git"
readonly TMP_DIR="$(mktemp -d "/tmp/${PROJECT_ARG//\//-}-release.XXXXXXXX")"
readonly REPO_DIR="${TMP_DIR}/repo"

function banner() {
  local color
  color=$(tput bold; tput setaf 4; tput rev)
  local reset
  reset=$(tput sgr0)
  echo "${color}$*${reset}"
}

function run() {
  echo "# $*" | paste -d' ' -s -
  if [[ "${FORCE_FLAG}" == "yes" ]]; then
    "$@"
  fi
}

function exit_handler() {
  if [[ -d "${TMP_DIR}" ]]; then
    banner "OOPS! Unclean shutdown"
    echo "Local repo at ${REPO_DIR}"
    echo 1
  fi
}
trap exit_handler EXIT

# We use github's "hub" command to create the release on on the GH website, so
# we make sure it's installed early on so we don't fail after completing part
# of the release. We also use 'hub' to do the clone so that the user is asked
# to authenticate at the beginning of the process rather than at the end.
if ! command -v hub > /dev/null; then
  die_with_message \
    "Can't find 'hub' command" \
    "Maybe run: sudo apt install hub" \
    "Or build it from https://github.com/github/hub"
fi

banner "Starting release for ${PROJECT_ARG} (${CLONE_URL})"
hub clone "${PROJECT_ARG}" "${REPO_DIR}"  # May force login to GH at this point
cd "${REPO_DIR}"

# Figures out the most recent tagged version, and computes the next version.
readonly TAG="$(git describe --tags --abbrev=0 origin/master)"
readonly CUR_TAG="$(test -n "${TAG}" && echo "${TAG}" || echo "v0.0.0")"
readonly CUR_VERSION="${CUR_TAG#v}"

NEW_VERSION=""
if [[ -n "${VERSION_ARG}" ]]; then
  NEW_VERSION="${VERSION_ARG}"
else
  # No new version specified; compute the new version number
  NEW_VERSION="$(perl -pe 's/(\d+).(\d+).(\d+)/"$1.${\($2+1)}.$3"/e' <<<"${CUR_VERSION}")"
fi
declare -r NEW_VERSION

# Avoid handling patch releases for now, because we wouldn't need a new branch
# for those.
if ! grep -P "\d+\.\d+\.0" <<<"${NEW_VERSION}" > /dev/null; then
  die_with_message "Sorry, cannot handle patch releases (yet)" "${USAGE}"
fi

readonly NEW_TAG="v${NEW_VERSION}"
readonly NEW_BRANCH="${NEW_TAG%.0}.x"

banner "Release info for ${CUR_TAG} -> ${NEW_TAG}"
echo "    New tag: ${NEW_TAG}"
echo " New branch: ${NEW_BRANCH}"

banner "Creating and pushing tag ${NEW_TAG}"
run git tag "${NEW_TAG}"
run git push origin "${NEW_TAG}"

banner "Creating and pushing branch ${NEW_BRANCH}"
run git checkout -b "${NEW_BRANCH}" "${NEW_TAG}"
run git push --set-upstream origin "${NEW_BRANCH}"

# Maybe todo: extract the release notes from the CHANGELOG.md file and stick
#             them in the release body.
banner "Creating release"
run hub release create \
  --prerelease \
  --message="${NEW_TAG} Release" \
  --message="*Paste release notes here*" \
  "${NEW_TAG}"

banner "Success!"
readonly release_fmt="
   date: %cI
    url: %U
tarball: %uT
zipball: %uZ
  state: %S
  title: %t
   body: %b
"
run hub release show --format="${release_fmt}" "${NEW_TAG}"

# Clean up
if [[ "${TMP_DIR}" == /tmp/* ]]; then
  rm -rf "${TMP_DIR}"
fi
