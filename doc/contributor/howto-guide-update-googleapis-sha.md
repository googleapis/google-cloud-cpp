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

## Find out the commit SHA you want to use

Use your browser to find out a recent commit SHA for `googleapis`.  In this
document we will use `134d8be4b58b2bfc373374f71ec52a0df59af664` as an example.
That was a recent SHA circa 2022-05-18.

Find the SHA256 of the repository tarball at that commit:

```shell
SHA=134d8be4b58b2bfc373374f71ec52a0df59af664
curl -sSL https://github.com/googleapis/googleapis/archive/${SHA}.tar.gz | sha256sum -
# Output: bf45ef870b4d36de2c7a9eebdd887cd8f9c6c016b65561c93a1f135bb577571a  -
```

Make a note of the download SHA256.

## Edit the code

You need to edit `cmake/GoogleapisConfig.cmake`
and `bazel/google_cloud_cpp_deps.bzl`.  You can see sample patches in the
[appendix](#appendix-sample-patches) at the end of this document.

Commit your edits:

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

## Appendix: Sample Patches

For `bazel/google_cloud_cpp_deps.bzl`:

```diff
diff --git a/bazel/google_cloud_cpp_deps.bzl b/bazel/google_cloud_cpp_deps.bzl
index 0f1f3abb9..890875c86 100644
--- a/bazel/google_cloud_cpp_deps.bzl
+++ b/bazel/google_cloud_cpp_deps.bzl
@@ -85,10 +85,10 @@ def google_cloud_cpp_deps():
         http_archive(
             name = "com_google_googleapis",
             urls = [
-                "https://github.com/googleapis/googleapis/archive/cf88418a2dea51a1007792ad19739d53d4ee510e.tar.gz",
+                "https://github.com/googleapis/googleapis/archive/134d8be4b58b2bfc373374f71ec52a0df59af664.tar.gz",
             ],
-            strip_prefix = "googleapis-cf88418a2dea51a1007792ad19739d53d4ee510e",
-            sha256 = "17c34d62592df2c684c19102079c4146faaafdba8244f517d2a52848a3e6b8b7",
+            strip_prefix = "googleapis-134d8be4b58b2bfc373374f71ec52a0df59af664",
+            sha256 = "bf45ef870b4d36de2c7a9eebdd887cd8f9c6c016b65561c93a1f135bb577571a",
             build_file = "@com_github_googleapis_google_cloud_cpp//bazel:googleapis.BUILD",
             # Scaffolding for patching googleapis after download. For example:
             #   patches = ["googleapis.patch"]
```

For `cmake/GoogleapisConfig.cmake`:

```diff
diff --git a/cmake/GoogleapisConfig.cmake b/cmake/GoogleapisConfig.cmake
index e3f6f2424..fdc1ca901 100644
--- a/cmake/GoogleapisConfig.cmake
+++ b/cmake/GoogleapisConfig.cmake
@@ -17,11 +17,11 @@
 # Give application developers a hook to configure the version and hash
 # downloaded from GitHub.
 set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_COMMIT_SHA
-    "cf88418a2dea51a1007792ad19739d53d4ee510e"
+    "134d8be4b58b2bfc373374f71ec52a0df59af664"
     CACHE STRING "Configure the commit SHA (or tag) for the googleapis protos.")
 mark_as_advanced(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA)
 set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
-    "17c34d62592df2c684c19102079c4146faaafdba8244f517d2a52848a3e6b8b7"
+    "bf45ef870b4d36de2c7a9eebdd887cd8f9c6c016b65561c93a1f135bb577571a"
     CACHE STRING "Configure the SHA256 checksum of the googleapis tarball.")
 mark_as_advanced(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256)

```

[googleapis-repo]: https://github.com/googleapis/googleapis.git
