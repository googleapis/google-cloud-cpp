# How-to Guide: Running CI builds locally

The `google-cloud-cpp` libraries need to be tested with different compilers,
different build tools, as shared libraries, as static libraries, and we need to
verify our instructions to install the libraries are up to date. This is in
addition to following best practices for C++, such as running the code with
dynamic analysis tools (e.g. AddressSanitizer), and enforcing style guidelines
with `clang-tidy`.

Each one of these builds may require different compilers, different system
libraries, or may require different ways to compile the dependencies.
Fortunately, one can use containers to create isolated environments to run
each one of these builds, and these containers can be created as-needed in both
the CI systems and the developer workstation. Well, at least as long as the
base platform supports containers.

## Running the CI build

If you just want to run one of the CI builds based on CMake then just install
Docker (once):

```console
# Make sure the docker server and client are installed in your workstation, you
# only need to do this once:
sudo apt update && sudo apt install docker.io
```

and run the same script the CI build uses:

```console
cd $HOME/google-cloud-cpp
ci/cloudbuild/build.sh -t clang-tidy-pr --docker
```

The build takes a few minutes the first time, as it needs to create a Docker
image with all the development tools and dependencies installed. Future builds
(with no changes to the source) take only a few seconds.

NOTE: Look in `ci/cloudbuild/triggers` for a list of other builds you can run.

## Getting a shell

Sometimes it is convenient to obtain a shell in the same environment where the
build will run, you can do this locally, using these commands:

```shell
cd $HOME/google-cloud-cpp
ci/cloudbuild/build.sh -t clang-tidy-pr --docker
ci/cloudbuild/build.sh -t clang-tidy-pr --docker -s  # --docker is implied by -s and is optional
```

The first `build.sh` will configure the docker image and environment and
compile as much of the code as possible. The second command will drop you into
a shell with the same tools, dependencies, and OS image as the CI build would
use. The source code is mounted in `/workspace` (your CWD), there is a fake
home directory in `/h`. The binary output from CMake builds is in `cmake-out/`.
