# Creating a new release of google-cloud-cpp

Unless there are no changes, we create releases for `google-cloud-cpp` at the
beginning of each month, usually on the first business day. We also create
releases if there is a major announcement or change to the status of one of the
libraries (like reaching the "Alpha" or "Beta" milestone).

The intended audience of this document are developers in the `google-cloud-cpp`
project that need to create a new release. We expect the reader to be familiar
the project itself, [git][git-docs], [GitHub][github-guides], and
[semantic versioning](https://semver.org).

## Preparing for a release

To create a new release you need to perform some maintenance tasks, these are
enumerated below.

### Verify CI passing

Before beginning the release process, verify all CI builds are passing on the
`main` branch. This is displayed in the GitHub page for the project.

### Update the root CMakeLists.txt

Set the pre-release version (PROJECT_VERSION_PRE_RELEASE) to the empty string.

```
set(PROJECT_VERSION_PRE_RELEASE "")
```

### Update CHANGELOG.md

To update the [`CHANGELOG.md`] file, first change the "TBD" placeholder in the
latest release header to the current YYYY-MM.

Then run the script

```bash
release/changes.sh
```

to output a summary of the potentially interesting changes since the previous
release. Paste that output below the release header updated above, and manually
tweak as needed.

- A change in an existing library warrants its own library section.
- Library sections should be listed in alphabetical order (Update `sections` in
  `release/changes.sh`).
- Do not list changes for libraries under development.
- Do not list changes for internal components.
- A change that affects all libraries should only be documented in the
  `Common Libraries` section.

### Send a PR with all these changes

In general, do not create the release branch before this PR is *merged*. We want
to create the release from a stable point in the default branch (`main`), and we
want this point to include the updated release notes and API baselines. There
may be exceptions to this guideline, you are encouraged to use your own
judgment.

## Creating the release

We next need to create the release tag, the release branch, and create the
release in the GitHub UI. We use a script ([`release/release.sh`]) to automate
said steps.

*No PR is needed for this step.*

First run the following command -- which will *NOT* make any changes to the repo
-- and verify that the output and *version numbers* look correct.

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

Review the new release in the GitHub web UI (the link to the pre-release will be
output from the `release.sh` script that was run in the previous step). If
everything looks OK:

1. Uncheck the pre-release checkbox.
1. Check the latest release checkbox.
1. Click the update release button.

## Check the published docs on googleapis.dev

The `publish-docs` build should start automatically when you create the release
branch. This build will upload the docs for the new release to the following
URLs:

- https://cloud.google.com/cpp/docs/reference/

It can take up to a day after the build finishes for the new docs to show up at
the above URL. You can watch the status of the build at
https://console.cloud.google.com/cloud-build/builds;region=us-east1?project=cloud-cpp-testing-resources&query=tags%3D%22publish-docs%22

## Bump the version number in `main`

Working in your fork of `google-cloud-cpp`: bump the version numbers to the
*next* version, i.e., one version past the release you just did above. Then send
the PR for review against `main`. You need to:

- In the top-level `CMakeLists.txt` file:
  - Increment the version number in the `project()` function.
  - Set the pre-release version (PROJECT_VERSION_PRE_RELEASE) to "rc".
- In the `CHANGELOG.md` file:
  - Add a "vX.Y.Z - TBD" header, corresponding to the new version number.
- Update the ABI baseline to include the new version numbers in the inline
  namespace by running `ci/cloudbuild/build.sh -t check-api-pr`. This will leave
  the updated ABI files in `ci/abi-dumps`, and also update the
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

- The `Require status checks to pass before merging` option is set. This
  prevents merges into the release branches that break the build.

  - The `Require branches to be up to date before merging` sub-option is set.
    This prevents two merges that do not conflict, but nevertheless break if
    both are pushed, to actually merge.
  - _At a minimum_ the `cla/google`, `asan-pr`, and `clang-tidy-pr` checks
    should be marked as "required". You may consider adding additional builds if
    it would prevent embarrassing failures, but consider the tradeoff of merges
    blocked by flaky builds.

- The `Include administrators` checkbox is turned on, we want to stop ourselves
  from making mistakes.

- Turn on the `Restrict who can push to matching branches`. Only Google team
  members should be pushing to release branches.

## Push the release to Microsoft vcpkg

[PR#32391] is probably a good example of the changes you will need to make.

- Create a fork of https://github.com/Microsoft/vcpkg.git.
- Clone the fork and/or sync the `master` branch in your fork to the upstream
  version.
- Create a feature branch.
  ```shell
  git checkout -b google-cloud-cpp-update-to-v2.13.0
  ```
- Update the version and list of features (any new GA libraries) in these files:
  ```
  ports/google-cloud-cpp/portfile.cmake
  ports/google-cloud-cpp/vcpkg.json
  ```
- Commit the changes
  ```shell
  git commit -m"[google-cloud-cpp] update to latest release (v2.13.0)" ports
  ```
- Update the version information (you really do need two commits)
  ```shell
  ./vcpkg x-add-version --all --overwrite-version
  git commit --amend --no-edit .
  ```
- Remove any older versions
  ```shell
  ./bootstrap-vcpkg.sh
  ./vcpkg remove --outdated --recurse
  ```
- Test the changes
  ```shell
  ./vcpkg install 'google-cloud-cpp[*]'
  ```

## Monitor the automation on Conda

On [Conda](https://conda.io) things are mostly automated. A robot will create a
PR, similar to [PR#138]. If you want, subscribe to notifications in the
[conda feedstock repository] or just look at the PRs in that repository over the
next 24 hours.

## Push the release to Conan

(Optional) Create a PR to update the `google-cloud-cpp` package in
[Conan](https://conan.io). These PRs are more involved.

This package manager requires patches to our code. These patches need to be
updated on each release. Package management systems tend to apply patches with
very strict settings, so even small changes around the patches break them.
Sometimes one can use `patch(1)` manually, with looser settings, and use that to
update the patches.

[PR#17988] is probably a good example of the changes you will need to make.

- Start by creating a git repository based on the release, for example:
  ```shell
  curl -sSL https://github.com/googleapis/google-cloud-cpp/archive/v2.12.0.tar.gz | tar -C $HOME -zxf -
  git -C $HOME/google-cloud-cpp-2.13.0/ init
  git -C $HOME/google-cloud-cpp-2.13.0/ add .
  git -C $HOME/google-cloud-cpp-2.13.0/ commit -q -m"Prepare for conan patches"
  ```
- Create a fork of
  [conan-center-index](https://github.com/conan-io/conan-center-index.git)
- Clone the fork:
  ```shell
  git clone git@github.com:${GITHUB_USERNAME}/conan-center-index
  cd conan-center-index
  ```
- Apply the existing patches and use the modified code to generate new versions:
  ```shell
  env -C $HOME/google-cloud-cpp-2.13.0/ patch -p1 \
      <recipes/google-cloud-cpp/2.x/patches/2.12.0/001-use-conan-msvc-runtime.patch
  mkdir -p recipes/google-cloud-cpp/2.x/patches/2.13.0
  git -C $HOME/google-cloud-cpp-2.13.0/ diff \
      >recipes/google-cloud-cpp/2.x/patches/2.13.0/001-use-conan-msvc-runtime.patch
  git -C $HOME/google-cloud-cpp-2.13.0/ commit -m"001-use-conan-msvc-runtime.patch" .
  ```
- Create the support files:
  ```shell
  recipes/google-cloud-cpp/2.x/extract_dependencies.py \
      --source-folder=$HOME/google-cloud-cpp-2.13.0 \
      >recipes/google-cloud-cpp/2.x/components_2_13_0.py
  ```
- Manually patch these files:
  ```shell
  recipes/google-cloud-cpp/config.yml
  recipes/google-cloud-cpp/2.x/conandata.yml
  recipes/google-cloud-cpp/2.x/conanfile.py
  ```

# Creating a patch release of google-cloud-cpp on an existing release branch

In your development fork:

- We will use `BRANCH=v1.17.x` and `PATCH=v1.17.1` as an example.
- Set these shell variables to appropriate values.
- Create a new branch
  ```shell
  git branch chore-prepare-for-${PATCH}-release upstream/${BRANCH}
  git checkout chore-prepare-for-${PATCH}-release
  ```
- Bump the version numbers for the patch release
  - Update the minor version in the top-level `CMakeLists.txt` file.
  ```shell
  git commit -m"chore: prepare for ${PATCH}"
  ```
- If this is the first patch release for that branch, you need to update the GCB
  triggers.
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
- Update our [vcpkg port].

[conda feedstock repository]: https://github.com/conda-forge/google-cloud-cpp-feedstock
[git-docs]: https://git-scm.com/doc
[github-branch-settings]: https://github.com/googleapis/google-cloud-cpp/settings/branches
[github-guides]: https://guides.github.com/
[pr#138]: https://github.com/conda-forge/google-cloud-cpp-feedstock/pull/138
[pr#17988]: https://github.com/conan-io/conan-center-index/pull/17988
[pr#32391]: https://github.com/microsoft/vcpkg/pull/32391
[vcpkg port]: https://github.com/Microsoft/vcpkg/tree/master/ports/google-cloud-cpp
[`changelog.md`]: /CHANGELOG.md
[`release/release.sh`]: https://github.com/googleapis/google-cloud-cpp/blob/main/release/release.sh
