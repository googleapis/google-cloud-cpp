# How-to Guide: Update googleapis SHA

This document describes the steps required to update the commit SHA for the
[googleapis][googleapis-repo] repository. The document is intended for
contributors to the `google-cloud-cpp` libraries. It assumes you are familiar
with the build systems used in these libraries.

`google-cloud-cpp` depends on the proto files in the
[googleapis][googleapis-repo]. The build scripts in `google-cloud-cpp` are
pinned to a specific commit SHA of this repository. That avoids unexpected
breakage for us and our customers as `googleapis` makes changes. From time to
time we need to manually update this commit SHA. This document describes these
steps.

## Update the googleapis BCR module

### BCR repository

1. Create a fork of https://github.com/bazelbuild/bazel-central-registry if you
   don't already have one.
1. Update the main branch to latest.

### googleapis repository

1. Create a fork of googleapis/googleapis.
1. Clone/update your fork to the latest on the master branch.
1. Pick a recent commit SHA from `git log`.
1. Execute the googleapis script to publish to BCR:

```
COMMIT=<a recent commit SHA>
cd .bcr
./publish-to-bcr.sh --ref ${COMMIT}  -f <path to your fork>bazel-central-registry/
```

Once this PR is merged, you can update the googleapis BCR module in
`google-cloud-cpp` by following the steps below.

## Set your working directory

Go to whatever directory holds your clone of the project, for example:

```shell
cd $HOME/google-cloud-cpp
```

## Create a branch to make your changes

```shell
git checkout main
git checkout -b chore-update-googleapis-sha-circa-$(date +%Y-%m-%d)
```

## Run the "renovate.sh" script

The script updates the googleapis BCR module to the version you just published.
The version is typically in the format `0.0.0-<date>-<short SHA>`.

```shell
external/googleapis/renovate.sh 0.0.0-20260812-f76fc6dc
```

## Verify everything compiles

```shell
bazel build //google/cloud/...
ci/cloudbuild/build.sh -t cmake-install-pr
```

## Push the branch and create a pull request

```shell
git push --set-upstream origin "$(git branch --show-current)"
```

Then use your favorite workflow to create the PR.

## Next Steps

Consider the output of the last command in this sequence. You may want to open
bugs or PRs to add any new `*.proto` files to existing libraries.

```shell
bazel build //:grpc_utils
googleapis="$(bazel info output_base)/external/googleapis~/"
time comm -23 \
    <(git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;' | \
        xargs env -C ${googleapis} grep -l '^service ' | sort) \
    <(sed -n  '/service_proto_path:/ {s/.*_path: "//; s/"//p}' generator/generator_config.textproto | sort)
```

See [Find missing service proto files] to find out how this command works.

[find missing service proto files]: /doc/contributor/howto-guide-find-missing-service-protos.md
[googleapis-repo]: https://github.com/googleapis/googleapis.git
