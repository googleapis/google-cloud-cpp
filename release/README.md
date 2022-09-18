# Creating a new release of google-cloud-cpp

Unless there are no changes, we create releases for `google-cloud-cpp` at the
beginning of each month, usually on the first business day. We also create
releases if there is a major announcement or change to the status of one
of the libraries (like reaching the "Alpha" or "Beta" milestone).

The intended audience of this document are developers in the `google-cloud-cpp`
project that need to create a new release. We expect the reader to be familiar
the project itself, [git][git-docs], [GitHub][github-guides], and
[semantic versioning](https://semver.org).

## Preparing for a release

To create a new release you need to perform some maintenance tasks, these are
enumerated below.

### Verify CI passing

Before beginning the release process, verify all CI builds are passing on
the `main` branch. This is displayed in the GitHub page for the project.

### Update CHANGELOG.md

To update the top-level [`CHANGELOG.md`] file, you run the script

```bash
release/changes.sh
```

to output a summary of the potentially interesting changes since the previous
release. You paste the output into the relevant section in the `CHANGELOG.md`
file, and manually tweak as needed.

- A change in an existing library warrants its own library section.
- Library sections should be listed in alphabetical order (Update `sections` in `release/changes.sh`).
- Do not list changes for libraries under development.
- Do not list changes for internal components.
- A change that affects all libraries should only be documented in the `Common Libraries` section.

### Send a PR with all these changes

In general, do not create the release branch before this PR is *merged*. We want
to create the release from a stable point in the default branch (`main`), and
we want this point to include the updated release notes and API baselines.
There may be exceptions to this guideline, you are encouraged to use your own
judgment.

## Creating the release

We next need to create the release tag, the release branch, and create the
release in the GitHub UI. We use a script ([`release/release.sh`]) to automate
said steps.

*No PR is needed for this step.*

First run the following command -- which will *NOT* make any changes to the
repo -- and verify that the output and *version numbers* look correct.

```bash
release/release.sh googleapis/google-cloud-cpp
```

If the output from the previous command looks OK, rerun the command with the
`-f` flag, which will make the changes and push them to the remote repo.

```bash
release/release.sh -f googleapis/google-cloud-cpp
```

**NOTE:** This script can be run from any directory. It operates only on the
specified repo.

### Publish the release

Review the new release in the GitHub web UI (the link to the pre-release will
be output from the `release.sh` script that was run in the previous step). If
everything looks OK, uncheck the pre-release checkbox and publish.

## Check the published docs on googleapis.dev

The `publish-docs` build should start automatically when you create the release
branch. This build will upload the docs for the new release to the following
URLs:

- https://googleapis.dev/cpp/google-cloud-bigquery/latest/
- https://googleapis.dev/cpp/google-cloud-bigtable/latest/
- https://googleapis.dev/cpp/google-cloud-iam/latest/
- https://googleapis.dev/cpp/google-cloud-pubsub/latest/
- https://googleapis.dev/cpp/google-cloud-spanner/latest/
- https://googleapis.dev/cpp/google-cloud-storage/latest/
- https://googleapis.dev/cpp/google-cloud-common/latest/

It can take up to an hour after the build finishes for the new docs to show up
at the above URLs. You can watch the status of the build at
https://console.cloud.google.com/cloud-build/builds;region=us-east1?project=cloud-cpp-testing-resources&query=tags%3D%22publish-docs%22

## Bump the version number in `main`

Working in your fork of `google-cloud-cpp`: bump the version numbers to the
*next* version, i.e., one version past the release you just did above. Then
send the PR for review against `main`. You need to:

- Update the version number in the top-level `CMakeLists.txt` file.
- Update the ABI baseline to include the new version numbers in the inline
  namespace by running `ci/cloudbuild/build.sh -t check-api-pr`. This will
  leave the updated ABI files in `ci/abi-dumps`, and also update the
  `google/cloud/internal/version_info.h` file.

**NOTE:** The Renovate bot will automatically update the Bazel deps in the
quickstart `WORKSPACE.bazel` files after it sees the new release published.
Watch for this PR to come through, kick off the builds, approve, and merge it.

## Review the protections for the `v[0-9].*` branches

We use the [GitHub Branch Settings][github-branch-settings] to protect the
release branches against accidental mistakes. From time to time changes in the
release branch naming conventions may require you to change these settings.
Please note that we use more strict settings for release branches than for
`main`, in particular:

- We require at least one review, but stale reviews are dismissed.

- The `Require status checks to pass before merging` option is set.
  This prevents merges into the release branches that break the build.

  - The `Require branches to be up to date before merging` sub-option
    is set. This prevents two merges that do not conflict, but nevertheless
    break if both are pushed, to actually merge.
  - _At a minimum_ the `cla/google`, `asan-pr`, and `clang-tidy-pr` checks should
    be marked as "required". You may consider adding additional builds if it
    would prevent embarrassing failures, but consider the tradeoff of merges
    blocked by flaky builds.

- The `Include administrators` checkbox is turned on, we want to stop ourselves
  from making mistakes.

- Turn on the `Restrict who can push to matching branches`. Only Google team
  members should be pushing to release branches.

## Push the release to Microsoft vcpkg

Nudge `coryan@` to update our [vcpkg port]. These updates are not difficult, but
contributing to this repository requires SVP approval. If you want, you can
seek approval to make such updates yourself, just keep in mind that this may
take several weeks.

# Creating a patch release of google-cloud-cpp on an existing release branch

In your development fork:

- We will use `PATCH=v1.17.1` as an example, set that shell variable to an
  appropriate value.
- Create a new branch
  ```shell
  git branch chore-prepare-for-${PATCH}-release upstream/v1.17.x
  git checkout chore-prepare-for-${PATCH}-release
  ```
- Bump the version numbers for the patch release
  - Update the minor version in the top-level `CMakeLists.txt` file.
  ```shell
  git commit -m"chore: prepare for ${PATCH}"
  ```
- If this is the first patch release for that branch, you need to update the
  GCB triggers.
  - Update the Google Cloud Build trigger definitions to compile this branch:
    ```shell
    ci/cloudbuild/convert-to-branch-triggers.sh
    ```
  - Actually create the triggers in GCB:
    ```shell
    for trigger in $(git ls-files -- ci/cloudbuild/triggers/*.yaml ); do
      ci/cloudbuild/trigger.sh --import "${trigger}";
    done
    ```
  - Commit these changes:
    ```shell
    git commit -m"Updated GCB triggers" ci
    ```
- Update the API baselines and `google/cloud/internal/version_info.h`:
  ```shell
  ci/cloudbuild/build.sh -t check-api-pr
  ```
- Commit the changes:
  ```shell
  git commit -m"Update API/ABI baseline" ci
  ```
- Push the branch and then create a PR:
  ```shell
  git push --set-upstream origin "$(git branch --show-current)"
  ```

______________________________________________________________________

## Send this PR for review and merge it before continuing

- Create a new branch off the release branch, which now contains the new patch
  version and baseline ABI dumps.
  ```shell
  git fetch upstream
  git branch my-patch upstream/v1.17.x
  git checkout my-patch
  ```
- Create or cherry-pick commits with the desired changes.
- Update `CHANGELOG.md` to reflect the changes made.
- After merging the PR(s) with all the above changes, use the Release UI on
  GitHub to create a pre-release along with a new tag for the release.
- After review, publish the release.
- Nudge `coryan@` to update our [vcpkg port].

[git-docs]: https://git-scm.com/doc
[github-branch-settings]: https://github.com/googleapis/google-cloud-cpp/settings/branches
[github-guides]: https://guides.github.com/
[vcpkg port]: https://github.com/Microsoft/vcpkg/tree/master/ports/google-cloud-cpp
[`changelog.md`]: /CHANGELOG.md
[`release/release.sh`]: https://github.com/googleapis/google-cloud-cpp/blob/main/release/release.sh
