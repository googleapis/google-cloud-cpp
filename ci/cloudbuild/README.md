# Google Cloud Build

This directory contains the files needed for our GCB presubmit ("PR builds")
and postsubmit ("CI builds") builds. The `cloudbuild.yaml` file is the main
config file for cloud build. Cloud builds may be managed from the command line
with the `gcloud builds` command. To make this process easier, the `build.sh`
script can be used to submit builds to GCB, run builds locally in a docker
container, or even run builds directly in your local environment. See `build.sh
--help` for more details.

GCB triggers can be configured in the http://console.cloud.google.com/ UI, but
we prefer to configure them with SCM controlled YAML files that live in the
`triggers/` directory. Managing triggers can be done with the `gcloud` command
also, but we have the local `trigger.sh` script to make this process a bit
easier. See `trigger.sh --help` for more details.

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

In addition to the guiding principles above, we'd like to keep the build
scripts in `builds/` as simple as possible; ideally just a simple listing of
the commands that a human would type with minimal logic. Many of these build
scripts should be only a few lines. A little duplication is even OK if the
resulting build script is simpler.
