# How-to Guide: Update googleapis SHA

This document describes the steps required to update the commit SHA for the
[googleapis][googleapis-repo] repository. The document is intended for
contributors to the `google-cloud-cpp` libraries. It assumes you are familiar
with the build systems used in these libraries.

`google-cloud-cpp` depends on the proto files in the
[googleapis][googleapis-repo]. The build scripts in `google-cloud-cpp` are
pinned to a specific commit SHA of this repository.  That avoids unexpected
breakage for us and our customers as `googleapis` makes changes. From time to
time we need to manually update this commit SHA.  This document describes these
steps.

## Set your working directory

Go to whatever directory holds your clone of the project, for example:

```shell
cd $HOME/google-cloud-cpp
```

Create a branch to make your changes

```shell
git checkout main
git checkout -b chore-update-googleapis-sha-circa-$(date +%Y-%m-%d)
```

## Run the "renovate.sh" script to update the Bazel/CMake dependencies

```shell
external/googleapis/renovate.sh
```

Commit those edits:

```shell
git commit -m"chore: update googleapis SHA circa $(date +%Y-%m-%d)" bazel cmake
```

## Update the generated libraries

```shell
ci/cloudbuild/build.sh -t generate-libraries-pr
```

## Stage these changes

```shell
git add .
```

## Verify everything compiles

```shell
bazel build //google/cloud/...
# CMake caches the old protos, clear them before compiling
rm -fr build-out/*/cmake-out
ci/cloudbuild/build.sh -t cmake-install-pr
```

## Commit the generated changes

```shell
git commit -m"Regenerate libraries"
```

## Push the branch and create a pull request

```shell
git push --set-upstream origin "$(git branch --show-current)"
```

Then use your favorite workflow to create the PR.

[googleapis-repo]: https://github.com/googleapis/googleapis.git
