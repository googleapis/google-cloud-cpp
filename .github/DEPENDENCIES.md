# Semi-automatic Dependency Updates with Dependabot

This document describes how we automatically keep the dependencies in `google-cloud-cpp` up to date. The intended audience
are developers and maintainers of `google-cloud-cpp`.

## Problem Statement

We have dependency information sprinkled across multiple files, Bazel, CMake super build files, and Dockerfiles. It is
easy to miss new releases. It is also easy to forget what files need to be updated when a dependency changes.

## Background

We could run nightly builds that compile everything against "latest" and force us to update as-needed. This creates
more work than probably we need to, and it makes the builds more flaky than necessary.

[Dependabot] is a GitHub product to automatically update your dependencies. It can be configured to periodically examine
your repository and create pull requests if any dependency needs to be updated. `Dependabot` does not support Bazel,
CMake, and the Docker support is not exactly what we want. However there is a clever technique to improve this:

## Overview

The community has developed a [workaround][workaround-link] to handle these cases. At a high-level, this workaround is:

* Define your dependencies as steps in a GitHub action.
    * This action is only a placeholder, it is only enabled for a branch that does not exist.
* `dependabot` will create a PR to update these steps.
* The human reviewer for this PR needs to update the corresponding scripts.
    * Humans can edit the automatically generated PR, so all changes take place in a single PR.
    
This is the workflow we have adopted for `google-cloud-cpp`.

[Dependabot]:  https://dependabot.com/
[workaround-link]: https://github.com/agilepathway/label-checker/blob/master/.github/DEPENDENCIES.md#workaround-for-other-dependencies
