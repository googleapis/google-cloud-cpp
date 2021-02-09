# Working with Bazel and CMake

The Google Cloud C++ Client Libraries support application developers that use
either Bazel or CMake as their build system. This document describes how to
keep the two build systems in sync, and how that works behind the scenes.

The design is very simple, but requires a minimal of discipline to work.

## Keeping Bazel BUILD files and CMake in Sync

Google Cloud C++ Client Library developers should add new files, tests and
examples to the CMake files.  The CMake configuration process automatically
updates the corresponding `.bzl` files.

The Cloud C++ Client Library developer **must** commit both their manual changes
to the CMake files as well as the automatic changes to the `.bzl` files. If they
forget one of the CI builds will break.

## How this works

The Bazel BUILD files load the generated `.bzl` files to obtain the sources and
headers for each library target, as well as the list of unit tests, examples,
and integration tests for each library.  The `.bzl` files are simply Python
lists that the BUILD files inject in the correct places.

## Alternatives Considered

We could manually kept the BUILD files and CMake files in sync. That increases
the maintenance burden for the libraries, so we decided against it.

We could change CMake to generate BUILD files, after all CMake is a build-file
generator. That seemed like a much larger endeavor than we wanted to tackle.

We could have used a common source for both the Bazel and CMake files, like gRPC
does. That seemed more complicated than what we need at this time.
