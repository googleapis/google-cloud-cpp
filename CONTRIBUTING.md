# How to become a contributor and submit your own code

## Contributor License Agreements

We'd love to accept your patches! Before we can take them, we
have to jump a couple of legal hurdles.

Please fill out either the individual or corporate Contributor License Agreement
(CLA).

  * If you are an individual writing original source code and you're sure you
    own the intellectual property, then you'll need to sign an
    [individual CLA](https://developers.google.com/open-source/cla/individual).
  * If you work for a company that wants to allow you to contribute your work,
    then you'll need to sign a
    [corporate CLA](https://developers.google.com/open-source/cla/corporate).

Follow either of the two links above to access the appropriate CLA and
instructions for how to sign and return it. Once we receive it, we'll be able to
accept your pull requests.

## Contributing A Patch

1. Submit an issue describing your proposed change to the repo in question.
1. The repo owner will respond to your issue promptly.
1. If your proposed change is accepted, and you haven't already done so, sign a
   Contributor License Agreement (see details above).
1. Fork the desired repo, develop and test your code changes.
1. Ensure that your code adheres to the existing style in the sample to which
   you are contributing.
1. Ensure that your code has an appropriate set of unit tests which all pass.
1. Submit a pull request.

### More Information on Forks and Pull Requests

If you are just getting started with `git` and [GitHub](https://github.com),
this section might be helpful. In this project we use the (more or less)
standard [GitHub workflow][workflow-link]:

1. You create a [fork][fork-link] of [google-cloud-cpp][repo-link]. Then
   [clone][about-clone] that fork into your workstation:
   ```console
   git clone git@github.com:YOUR-USER-NAME/google-cloud-cpp.git
   ```
1. The cloned repo that you created in the previous step will have its `origin`
   set to your forked repo. You should now tell git about the the main
   `upstream` repo, which you'll use to pull commits made by others in order to
   keep your local repo and fork up to date.
   ```console
   git remote add upstream git@github.com:googleapis/google-cloud-cpp.git
   git remote -v  # Should show 'origin' (your fork) and 'upstream' (main repo)
   ```
1. To pull new commits from `upstream` into your local repo and
   [sync your fork][syncing-a-fork] you can do the following:
   ```console
   git checkout master
   git pull --ff-only upstream master
   git push  # Pushes new commits up to your fork on GitHub
   ```
   Note: You probably want to do this periodically, and almost certainly before
   starting your next task.
1. You pick an existing [GitHub bug][mastering-issues] to work on.
1. You start a new [branch][about-branches] for each feature (or bug fix).
   ```console
   git checkout master
   git checkout -b my-feature-branch
   git push -u origin my-feature-branch  # Tells fork on GitHub about new branch
   # make your changes
   git push
   ```
1. You submit a [pull-request][about-pull-requests] to merge your branch into
   `googleapis/google-cloud-cpp`.
1. Your reviewers may ask questions, suggest improvements or alternatives. You
   address those by either answering the questions in the review or adding more
   [commits][about-commits] to your branch and `git push` -ing those commits to
   your fork.
1. From time to time your pull request may have conflicts with the destination
   branch (likely `master`), if so, we request that you [rebase][about-rebase]
   your branch instead of merging. The reviews can become very confusing if you
   merge during a pull request. You should first ensure that your `master`
   branch has all the latest commits by syncing your fork (see above), then do
   the following:
   ```console
   git checkout my-feature-branch
   git rebase master
   git push --force-with-lease
   ```
1. If one of the CI builds fail please see below, most of the CI builds can
   be reproduced locally on your workstations using Docker.
1. Eventually the reviewers accept your changes, and they are merged into the
   `master` branch.

[workflow-link]: https://guides.github.com/introduction/flow/
[fork-link]: https://guides.github.com/activities/forking/
[repo-link]: https://github.com/googleapis/google-cloud-cpp.git
[mastering-issues]: https://guides.github.com/features/issues/
[about-clone]: https://help.github.com/articles/cloning-a-repository/
[about-branches]: https://help.github.com/articles/about-branches/
[about-pull-requests]: https://help.github.com/articles/about-pull-requests/
[about-commits]: https://help.github.com/desktop/guides/contributing-to-projects/committing-and-reviewing-changes-to-your-project/#about-commits
[about-rebase]: https://help.github.com/articles/about-git-rebase/
[syncing-a-fork]: https://help.github.com/articles/syncing-a-fork/

## Style

This repository follows the [Google C++ Style Guide](
https://google.github.io/styleguide/cppguide.html), with some additional
constraints specified in the [style guide](doc/cpp-style-guide.md).
Please make sure your contributions adhere to the style guide.

### Formatting

The code in this project is formatted with `clang-format(1)`, and our CI builds
will check that the code matches the format generated by this tool before
accepting a pull request. Please configure your editor or IDE to use the Google
style for indentation and other whitespace. If you need to reformat one or more
files, you can simply run `clang-format` manually:

```console
$ clang-format -i <file>....
```

Reformatting all the files in a specific directory should be safe too, for
example:

```console
$ find google/cloud \( -name '*.h' -o -name '*.cc' \) -print0 \
    | xargs -0 clang-format -i
```

You might find it convenient to reformat only the files that you added or
modified:
```console
$ git status -s \
    | awk 'NF>1{print $NF}' \
    | grep -E '.*\.(cc|h)$' \
    | while read fpath; do if [ -f "$fpath" ]; then echo "$fpath"; fi; done \
    | xargs clang-format -i
```

If you need to reformat one of the files to match the Google style.  Please be
advised that `clang-format` has been known to generate slightly different
formatting in different versions. We use version 7; use the same version if you
run into problems.

You can verify and fix the format of your code using:

```console
# Run from google-cloud-cpp:
$ ./ci/kokoro/docker/build.sh clang-tidy
```

### Updating CMakeLists.txt and/or BUILD files

If you need to change the list of files associated with a library, please change
the existing `CMakeLists.txt` files, even if you use Bazel as your preferred
build system.  Changing the CMake files automatically update the corresponding
`.bzl` files, but changing the `.bzl` files will not. More details in the short
[design doc](doc/working-with-bazel-and-cmake.md).

## Advanced Compilation and Testing

Please see the [README](README.md) for the basic instructions on how to compile
the code.  In this section we will describe some advanced options.

### Changing the Compiler

If your workstation has multiple compilers (or multiple versions of a compiler)
installed, you can change the compiler using:

```console
# Run this in your build directory, typically google-cloud-cpp/cmake-out
$ CXX=clang++ CC=clang cmake ..

# Then compile and test normally:
$ make -j 4
$ make -j 4 test
```

### Changing the Build Type

By default, the system is compiled with optimizations on; if you want to compile
a debug version, use:

```console
# Run this in your build directory, typically google-cloud-cpp/cmake-out
$ CXX=clang++ CC=clang cmake -DCMAKE_BUILD_TYPE=Debug ..

# Then compile and test normally:
$ make -j 4
$ make -j 4 test
```

This project supports `Debug`, `Release`, and `Coverage` builds.

### Using Docker to Compile and Test

This project uses Docker in its CI builds to test against multiple Linux
distributions, using the native compiler and library versions included in those
distributions.
From time to time, you may need to run the same build on your workstation.
To do so, [install Docker](https://docs.docker.com/engine/installation/)
on your workstation. Once that is done invoke the build script with the name
of the build you need to reproduce, for example:

```console
# Run from the google-cloud-cpp directory.
$ ./ci/kokoro/docker/build.sh shared
```

### Upload generated documentation to personal github pages

If you are using your personal fork as the git origin, there is an easy way to
upload generated doxygen documentation to your personal github pages. The
following command will generate the documentation and upload it to the
`my-fancy-feature` subdirectory in the gh-pages branch of your personal fork.
That is, the documentation will be publicly available at:
`http://<username>.github.io/google-cloud-cpp/my-fancy-feature`.

```console
$ DOCS_SUBDIR=my-fancy-feature ./ci/kokoro/docker/build.sh clang-tidy
```
