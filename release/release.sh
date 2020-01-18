#!/bin/bash
#
# Usage:
#   $ release.sh [-f] <organization/project-name>
#
#   Options:
#     -f     Force; actually make and push the changes
#     -h     Print help message
#
# Example:
#   # Shows what commands would be run. NO CHANGES ARE PUSHED
#   $ release.sh googleapis/google-cloud-cpp-spanner
#
#   # Shows commands AND PUSHES changes when -f is specified
#   $ release.sh -f googleapis/google-cloud-cpp-spanner
#
# This script creates a "release" on github by doing the following:
#   1. Computes the next version to use
#   2. Creates and pushes the tag w/ the new version
#   3. Creates and pushes a new branch w/ the new version
#   4. Creates the "Pre-Release" in the GitHub UI.
#
# Before running this script the user should make sure the README.md on master
# is up-to-date with the release notes for the new release that will happen.
# Then run this script. After running this script, the user must still go to
# the GH UI where the new release will exist as a "pre-release", and edit the
# release notes.

set -eu

# Extracts all the documentation at the top of this file as the usage text.
readonly USAGE="$(sed -n '3,/^$/s/^# \?//p' "$0")"

FORCE_FLAG="no"
while getopts "fh" opt "$@"; do
  case "$opt" in
    [f])
      FORCE_FLAG="yes";;
    [h])
      echo "$USAGE"
      exit 0;;
    *)
      echo "$USAGE"
      exit 1;;
  esac
done
shift $((OPTIND - 1))
declare -r FORCE_FLAG

if [[ $# -ne 1 ]]; then
  echo "Missing repo name, example googleapis/google-cloud-cpp-spanner"
  echo "$USAGE"
  exit 1;
fi

readonly PROJECT="$1"
readonly CLONE_URL="git@github.com:${PROJECT}.git"
readonly TMP_DIR="$(mktemp -d "/tmp/${PROJECT//\//-}-release.XXXXXXXX")"
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
if ! which hub > /dev/null; then
  echo "Can't find 'hub' command"
  echo "Maybe run: sudo apt install hub"
  echo "Or build it from https://github.com/github/hub"
  exit 1
fi

banner "Starting release for ${PROJECT} (${CLONE_URL})"
hub clone "${PROJECT}" "${REPO_DIR}"  # May force login to GH at this point
cd "${REPO_DIR}"

# Figures out the most recent tagged version, and computes the next version.
readonly TAG="$(git describe --tags --abbrev=0 origin/master)"
readonly CUR_TAG="$(test -n "${TAG}" && echo "${TAG}" || echo "v0.0.0")"
readonly NEW_RELEASE="$(perl -pe 's/v0.(\d+).0/"v0.${\($1+1)}"/e' <<<"${CUR_TAG}")"
readonly NEW_TAG="${NEW_RELEASE}.0"
readonly NEW_BRANCH="${NEW_RELEASE}.x"

banner "Release info for ${NEW_RELEASE}"
echo "Current tag: ${CUR_TAG}"
echo "    New tag: ${NEW_TAG}"
echo " New branch: ${NEW_BRANCH}"

banner "Creating and pushing tag ${NEW_TAG}"
run git tag "${NEW_TAG}"
run git push origin "${NEW_TAG}"

banner "Creating and pushing branch ${NEW_BRANCH}"
run git checkout -b "${NEW_BRANCH}" "${NEW_TAG}"
run git push --set-upstream origin "${NEW_BRANCH}"

# Maybe todo: extract the release notes from the README.md file and stick them
#             in the release body.
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
