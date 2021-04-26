# Contributor Guides

Thank you for contributing to `google-cloud-cpp`. We receive contributions from
folks with very different backgrounds. Some of you are experienced C++
developers, but this might be your first contribution to a GitHub hosted
project. Some of you may be experienced GitHub contributors, but this might be
the first time contributing to a C++ library project. Some of you may be
experienced with both, but `google-cloud-cpp` has special requirements to
support multiple platforms, compilers, and even multiple build tools.

To help you navigate the project we have prepared (over time) a number of
documents. You do not need to read each one, specially if they address areas
you are already familiar with, but you may want to browse them in case something
is surprising (or wrong! We need to improve these documents too).

## Key Documents

* [Forks and Pull Requests](howto-guide-forks-and-pull-requests.md): the basic
  workflows for GitHub projects.
* [Running CI builds locally](howto-guide-running-ci-builds-locally.md): how to
  reproduce CI results locally, for faster edit -> build -> test cycles.
* [Setup a development workstation](howto-guide-setup-development-workstation.md):
  how to setup a Linux or Windows (workstation or VM) for `google-cloud-cpp`
  development.
* [Setup CMake Dependencies](howto-guide-setup-cmake-environment.md): how to
  install the dependencies of `google-cloud-cpp` in `$HOME` for easier and
  faster development with CMake.
* [Working with Bazel and CMake](working-with-bazel-and-cmake.md): this project
  can be compiled with CMake or Bazel. Always update the CMake project files
  first, as these files are used to generate `*.bzl` files loaded by Bazel.
  More details in the linked document.

## Style

This repository follows the [Google C++ Style Guide](
https://google.github.io/styleguide/cppguide.html), with some additional
constraints specified in the [style guide](/doc/cpp-style-guide.md).
Please make sure your contributions adhere to the style guide.

Many of these rules (but not all of them) are enforced using `clang-tidy(1)`.

## Formatting

We have a CI build that will check that all code is properly formatted. Our C++
code is formatted using `clang-format(1)` with our top-level `.clang-format`
file, which you should be able to configure your editor or IDE to use. To use
our format-checker build to format your code you must first set up your
workstation to [run CI builds
locally](howto-guide-running-ci-builds-locally.md), and then run the following
command:

```console
$ ci/cloudbuild/build.sh -t checkers-pr --docker
```

NOTE: Please be advised that `clang-format` has been known to generate slightly
different formatting between different versions. We update this version from
time to time. As of 2021-02-09 we are using clang-format version 11. If you
need to confirm the exact version we're using, look at the
[Dockerfile](/ci/cloudbuild/dockerfiles/fedora.Dockerfile) used in the
`checkers-pr` build, find out which version of Fedora is in use, and then find
out what is the `clang` version included in that distro: search on pkgs.org,
repology.org, or Google; or just ask around.

## Generating doxygen documentation

To generate the doxygen HTML pages on your local machine, do the following:

```console
$ ci/cloudbuild/build.sh -t publish-docs-pr --docker

# The docs live in the "html" directories
$ find build-out/fedora-publish-docs/cmake-out/ -name html
```
