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

```shell
external/googleapis/renovate.sh
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
googleapis="$(bazel info output_base)/external/com_google_googleapis/"
time comm -23 \
    <(git ls-files -- 'external/googleapis/protolists/*.list' | \
        xargs sed -e 's;@com_google_googleapis//;;' -e 's;:;/;' | \
        xargs env -C ${googleapis} grep -l '^service ' | sort) \
    <(sed -n  '/service_proto_path:/ {s/.*_path: "//; s/"//p}' generator/generator_config.textproto | sort)
```

See [Find missing service proto files] to find out how this command works.

[find missing service proto files]: /doc/contributor/howto-guide-find-missing-service-protos.md
[googleapis-repo]: https://github.com/googleapis/googleapis.git
