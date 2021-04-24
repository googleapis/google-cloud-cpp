# Google Cloud Build

This directory contains the files needed for our GCB presubmit ("PR builds")
and postsubmit ("CI builds") builds. The `cloudbuild.yaml` file is the main
config file for Google Cloud Build (GCB). Cloud builds may be managed from the
command line with the `gcloud builds` command. To make this process easier, the
`build.sh` script can be used to submit builds to GCB, run builds locally in a
docker container, or even run builds directly in your local environment. See
`build.sh --help` for more details.

Build scripts live in the `builds/` directory. We want to keep these scripts as
simple as possible. Ideally they will be just a simple listing of the commands
that a human would type with minimal logic. Many of these build scripts should
be only a few lines. A little duplication is even OK if the resulting build
script is simpler. Sometimes there are non-trivial things we need to do
repeatedly (e.g., run the quickstart builds), and these live in `builds/lib/`.

Each build runs in an environment defined by a Dockerfile in the `dockerfiles/`
directory. Some Dockerfiles are used by multiple builds, some only one. Build
scripts and Dockerfiles are associated in a trigger file.

GCB triggers can be configured in the http://console.cloud.google.com/ UI, but
we prefer to configure them with version controlled YAML files that live in the
`triggers/` directory. Managing triggers can be done with the `gcloud` command
also, but we have the local `trigger.sh` script to make this process a bit
easier. See `trigger.sh --help` for more details.

The internal-only doc at http://go/cloud-cxx:gcb-project has more info about
how we've configured the Google Cloud project where these builds run.

## Adding a new build

Adding a new build is pretty simple, and can be done in a single PR. For
example, see https://github.com/googleapis/google-cloud-cpp/pull/6252, which
adds the `cxx17-pr` and `cxx17-ci` builds. The steps to add a new build are:

1. (Optional) If your build needs to run in a unique environment, you should
   start by creating a new Dockerfile in `dockerfiles/`. If you don't need a
   specific environment, we typically use the `dockerfiles/fedora.Dockerfile`
   image, which is simply called the "fedora" distro. Here's an [example
   PR](https://github.com/googleapis/google-cloud-cpp/pull/6259) that adds a
   build with its own Dockerfile.
2. Create the new build script in `builds/` that runs the commands you want.
   * You can test this script right away with `build.sh` by explicitly
     specifying the `--distro` you want your build to run in.  For example:
   ```
   $ build.sh --distro fedora my-new-build # or ...
   $ build.sh --distro fedora my-new-build --docker
   ```
3. Create your trigger file(s) in the `triggers/` directory. If you want both
   PR (presubmit) and CI (postsubmit) builds you can generate the trigger files
   with the command `trigger.sh --generate my-new-build`, which will write the
   new files in the `triggers/` directory. You may need to tweak the files at
   this point, for example to change the distro (fedora is the default).
4. At this point, you're pretty much done. You can now test your build using
   the trigger name as shown here:
   ```
   $ build.sh -t my-new-build-pr # or ...
   $ build.sh -t my-new-build-pr --docker # or ...
   $ build.sh -t my-new-build-pr --project cloud-cpp-testing-resources
   ```
5. Send a PR! Google Cloud Build will not know about your new trigger files yet
   so they will not be run for your PR. This is working as intended. Get the PR
   reviewed and merge it.
6. FINAL STEP: Now that the code for your new build is checked in, tell GCB
   about the triggers so the can be run automatically for all future PRs and
   merges.
   ```
   $ trigger.sh --import triggers/my-new-build-pr.yaml
   $ trigger.sh --import triggers/my-new-build-ci.yaml
   ```

## Testing principles

We want our code to work for our customers. We don't know their exact
environment and configuration, so we need to test our code in a variety of
different environments and configurations. The main dimensions that we need to
test are:

- OS Platform: Windows, macOS, Linux x `N` different distros
- Compilers: Clang, GCC, MSVC
- Build tool: Bazel, CMake
- Configuration: release, debug, with/without exceptions, dynamic vs static
- C++ Language: C++11, ..., C++20
- Installation: CMake, pkg-config

In addition to these main dimensions, we also want to use tools and analyzers
to help us catch bugs: clang-tidy, clang static analyzer, asan, msan, tsan,
ubsan, etc. The full matrix of all combinations is infeasible to test
completely, so we follow the following principles to minimize the test space
while achieving high likelihood of the code working for our customers.

* For simple dimensions (e.g. things that are "on/off") we want at least one
  build for each 'value' of the setting.
* On dimensions with versions, we want to test something _old_ and something
  _new_ (specific versions will change over time)
* Integration tests should prefer running against an emulator in _most_ cases
* Integration tests should hit production (i.e., no emulator) somewhere, though
  we need to be careful of quotas
* Code coverage builds need to run integration tests
* Sanitizer builds need to run integration tests
* We want to test our user-facing instructions (i.e., how to install and use)
  as much as possible (this is difficult on macOS and Windows without docker)
