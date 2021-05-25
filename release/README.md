# Creating a new release of google-cloud-cpp

Unless there are no changes, we create releases for `google-cloud-cpp` every
4 weeks, or if there is a major announcement or change to the status of one
of the libraries (like reaching the "Alpha" or "Beta" milestone).

The intended audience of this document are developers in the `google-cloud-cpp`
project that need to create a new release. The audience is expected to be
familiar with the project itself, [git][git-docs], [GitHub][github-guides],
[semantic versioning](https://semver.org).

## Preparing for a release

Assuming you are working on your own fork of the `google-cloud-cpp` project,
and `upstream` points to the `googleapis/google-cloud-cpp` remote, these
commands should be useful in identifying important changes for inclusion in the
release notes.

### Verify CI passing
Before beginning the release process, verify all CI builds are passing on
main.

### Update the API baseline

Run the `check-api` build to update the API baseline. Once you cut the release
any new APIs are, well, released, and we should think carefully about removing
them.

```bash
ci/cloudbuild/build.sh -t check-api-pr --docker
```

The updated ABI dump files will be left in the `ci/abi-dumps` folder.

This may take a while, leave it running while you perform the next step. You
can, but are not required to, send a single PR to update the baseline and the
`CHANGELOG.md` file.

### Update CHANGELOG.md

Update `CHANGELOG.md` based on the release notes for Bigtable, Storage,
Spanner, and the common libraries:

```bash
# Summarize the output of this into CHANGELOG.md under the "Bigtable" header
git log --no-merges --format="format:* %s" \
    $(git describe --tags --abbrev=0 upstream/main)..HEAD \
    upstream/main -- google/cloud/bigtable
```

```bash
# Summarize the output of this into CHANGELOG.md under the "Pub/Sub" header
git log --no-merges --format="format:* %s" \
    $(git describe --tags --abbrev=0 upstream/main)..HEAD \
    upstream/main -- google/cloud/pubsub
```

```bash
# Summarize the output of this into CHANGELOG.md under the "Storage" header
git log --no-merges --format="format:* %s" \
    $(git describe --tags --abbrev=0 upstream/main)..HEAD \
    upstream/main -- google/cloud/storage
```

```bash
# Summarize the output of this into CHANGELOG.md under the "Spanner" header
git log --no-merges --format="format:* %s" \
    $(git describe --tags --abbrev=0 upstream/main)..HEAD \
    upstream/main -- google/cloud/spanner
```

```bash
# Summarize the output of this into CHANGELOG.md under the "Common libraries" header
git log --no-merges --format="format:* %s" \
    $(git describe --tags --abbrev=0 upstream/main)..HEAD \
    upstream/main -- google/cloud \
   ':(exclude)google/cloud/firestore/' \
   ':(exclude)google/cloud/bigtable/' \
   ':(exclude)google/cloud/pubsub/' \
   ':(exclude)google/cloud/spanner/' \
   ':(exclude)google/cloud/storage/'
```

Any **chore**/**ci**/**test**-tagged PRs in the above lists should probably be
discarded as they are uninteresting to our users.

### Send a PR with all these changes

It is not recommended that you create the release branch before this PR is
*merged*, but in some circumstances it might be needed, for example, if a large
change that could destabilize the release is about to be merged, or if we want
to create the release at an specific point in the revision history.

## Creating the release

We next need to create the release tag, the release branch, and create the
release in the GitHub UI. These steps are handled automatically for us by the
[`release/release.sh`
script](https://github.com/googleapis/google-cloud-cpp/blob/main/release/release.sh).

*No PR is needed for this step.*

First run the following command -- which will *NOT* make any changes to any
repos -- and verify that the output and *version numbers* look correct.

```bash
$ release/release.sh googleapis/google-cloud-cpp
```

If the output from the previous command looks OK, rerun the command with the
`-f` flag, which will make the changes and push them to the remote repo.

```bash
$ release/release.sh -f googleapis/google-cloud-cpp
```

**NOTE:** This script can be run from any directory. It operates only on the
specified repo.

### Publish the release

Review the new release in the GitHub web UI (the link to the pre-release will
be output from the `release.sh` script that was run in the previous step). If
everything looks OK, uncheck the pre-release checkbox and publish.

## Check the published docs on googleapis.dev

After the release branch is submitted, the `publish-docs` build will be
automatically triggered, and it will upload the docs for the new release to the
following URLs:

* https://googleapis.dev/cpp/google-cloud-bigtable/latest/
* https://googleapis.dev/cpp/google-cloud-pubsub/latest/
* https://googleapis.dev/cpp/google-cloud-storage/latest/
* https://googleapis.dev/cpp/google-cloud-spanner/latest/
* https://googleapis.dev/cpp/google-cloud-common/latest/

It can take up to an hour after the build finishes for the new docs to show up
at the above URLs. You can watch the status of the build at
https://console.cloud.google.com/cloud-build/builds?project=cloud-cpp-testing-resources&query=tags%3D%22publish-docs%22

## Bump the version numbers in `main`

Working in your fork of `google-cloud-cpp`: bump the version numbers to the
*next* version (i.e., one version past the release you just did above), and
send the PR for review against `main` You need to:

- Update the version numbers in the top-level `CMakeLists.txt` file.
- Run the cmake configuration step, this will update the different
  `version_info.h` files.
- Update the version number and SHA256 checksums in the
  `google/cloud/*/quickstart/WORKSPACE` files to point to the *just-released*
  version.

## Review the protections for the `v[0-9].*` branches

We use the [GitHub Branch Settings][github-branch-settings] to protect the
release branches against accidental mistakes. From time to time changes in the
release branch naming conventions may require you to change these settings.
Please note that we use more strict settings for release branches than for
`main`, in particular:

* We require at least one review, but stale reviews are dismissed.
* The `Require status checks to pass before merging` option is set.
  This prevents merges into the release branches that break the build.
  * The `Require branches to be up to date before merging` sub-option
    is set. This prevents two merges that do not conflict, but nevertheless
    break if both are pushed, to actually merge.
  * _At a minimum_ the `cla/google`, `asan-pr`, and `clang-tidy-pr` checks are
    required to pass, but more may be selected.

* The `Include administrators` checkbox is turned on, we want to stop ourselves
  from making mistakes.

* Turn on the `Restrict who can push to matching branches`. Only Google team
  members should be pushing to release branches.

[git-docs]: https://git-scm.com/doc
[github-guides]: https://guides.github.com/
[github-branch-settings]: https://github.com/googleapis/google-cloud-cpp/settings/branches

## Push the release to Microsoft vcpkg

Nudge coryan@ to send a PR to
[vcpkg](https://github.com/Microsoft/vcpkg/tree/master/ports/google-cloud-cpp).
The PRs are not difficult, but contributing to this repository requires SVP
approval.

# Creating a patch release of google-cloud-cpp on an existing release branch

In your development fork:
* Switch to the existing release branch, e.g. `git checkout v1.17.x`.
* Create a new branch off the release branch.
* Create or cherry-pick commits with the desired changes.
* Update `CHANELOG.md` to reflect the changes made.
* Bump the minor version number in the top-level CMakeLists.txt and run the
cmake configuration step to update the different `version_info.h` files.
* After merging the PR(s) with all the above changes, use the Release gui on
github to create a pre-release along with a new tag for the release.
* After review, publish the release.
* Nudge coryan@ to send a PR to
[vcpkg](https://github.com/Microsoft/vcpkg/tree/master/ports/google-cloud-cpp).
