# How-to Guide: Set Up your Development Environment

This guide is intended for contributors to the `google-cloud-cpp` libraries.
This will walk you through the steps necessary to set up your development
workstation to compile the code, run the unit and integration tests, and send
contributions to the project.

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers who prefer using a package manager such as
  [vcpkg](https://vcpkg.io), or [Conda](https://conda.io), should follow the
  instructions for their package manager.
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](/README.md#quickstart) for the
  library or libraries they want to use.
- Developers wanting to compile the library just to run some examples or tests
  should consult the
  [building and installing](/README.md#building-and-installing) section of the
  top-level README file.
- Contributors and developers **to** `google-cloud-cpp`, this is the right
  document.

## Table of Contents

- [Linux](#linux)
- [Windows](#windows)
- [macOS](#macos)
- [Appendix: Linux VM on Google Compute Engine][appending-linux]
- [Appendix: Windows VM on Google Compute Engine][appending-windows]

## Linux

Install the dependencies needed for your distribution. The top-level
[README](/README.md) file lists the minimal development tools necessary to
compile `google-cloud-cpp`. But for active development you may want to install
additional tools to run the unit and integration tests.

These instructions will describe how to install these tools for Ubuntu 18.04
(Bionic Beaver). For other distributions you may consult the Dockerfiles in
[ci/cloudbuild/dockerfiles/](https://github.com/googleapis/google-cloud-cpp/tree/main/ci/cloudbuild/dockerfiles)
If you use a different distribution, you will need to use the corresponding
package manager (`dnf`, `zypper`, `apk`, etc.) and find the corresponding
package names.

Install the basic development tools:

```sh
sudo curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.18.0/bazelisk-linux-amd64"
sudo chmod +x /usr/bin/bazelisk
sudo ln -s /usr/bin/bazelisk /usr/bin/bazel

sudo apt-get update
sudo apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential cmake ca-certificates curl git \
        gcc g++ libc-ares-dev libc-ares2 libcurl4-openssl-dev libre2-dev \
        libssl-dev m4 make pkg-config tar wget zlib1g-dev
sudo apt install -y ninja-build
sudo apt-get install curl zip unzip tar
```

Install docker and its `buildx` component:

```sh
sudo apt install -y docker.io docker-buildx
```

Configure your user to launch docker without needing `sudo`:

```sh
sudo usermod -aG docker $USER
```

Update all packages and reboot:

```sh
sudo apt upgrade
sudo shutdown -r now
```

Verify docker is working:

```sh
docker run hello-world
```

Clone the repository and run a test build:

```sh
git clone git@github.com:${GITHUB_USER}/google-cloud-cpp.git
```

```sh
time env -C google-cloud-cpp bazel build //google/cloud/storage/...
#
# real    6m36.547s
# user    0m0.825s
# sys     0m0.630s
```

You need to install the Google Cloud SDK. These instructions work for a GCE VM,
but you may need to adapt them for your environment. Check the instructions on
the [Google Cloud SDK website](https://cloud.google.com/sdk/) for alternatives.

```sh
type gcloud || sudo google-cloud-cpp/ci/install-cloud-sdk.sh
```

By default, `gcloud` will use the GCE instance credentials. That may be enough
in most cases, but you may need to use your *own* credentials:

```sh
/usr/local/google-cloud-sdk/bin/gcloud auth login
```

Don't forget the second ~breakfast~ login:

```sh
/usr/local/google-cloud-sdk/bin/gcloud auth application-default login
```

Test checkers:

```sh
time env -C google-cloud-cpp ci/cloudbuild/build.sh -t checkers-pr
# The first time is slow, as it needs to create the docker image. The second
# time it should run
# real    0m30.997s
# user    0m0.282s
# sys     0m0.232s
```

```sh
(
echo ENABLED_FEATURES=storage,storage_grpc,opentelemetry
echo RUN_CLANG_TIDY_FIX=NO
) >$HOME/.cloudcxxrc
time env -C google-cloud-cpp ci/cloudbuild/build.sh --distro fedora-latest-cmake --build development
# The second run should get times such as
# real    1m48.347s
# user    0m0.259s
# sys     0m0.247s
```

### Optional Tools

Most of the time we use these tools in docker images, so there is no need to
install them in your development environment.

Install the buildifier tool, which we use to format `BUILD.bazel` files:

```sh
sudo curl -fsSL -o /usr/bin/buildifier https://github.com/bazelbuild/buildtools/releases/download/6.0.0/buildifier-linux-amd64
sudo chmod 755 /usr/bin/buildifier
```

Install shfmt tool, which we use to format our shell scripts:

```sh
sudo curl -L -o /usr/local/bin/shfmt \
    "https://github.com/mvdan/sh/releases/download/v3.4.3/shfmt_v3.4.3_linux_amd64"
sudo chmod 755 /usr/local/bin/shfmt
```

Install `cmake_format` to automatically format the CMake list files. We pin this
tool to a specific version because the formatting changes when the "latest"
version is updated, and we do not want the builds to break just because some
third party changed something.

```sh
sudo apt install -y python3 python3-pip
pip3 install --upgrade pip
pip3 install cmake_format==0.6.13
```

Install \`black\`\`, which we use to format our Python scripts:

```sh
pip3 install black==22.3.0
```

Install `mdformat`, which we use to format our `.md` documents:

```sh
pip3 install mdformat-gfm==0.3.5 \
                 mdformat-frontmatter==0.4.1 \
                 mdformat-footnote==0.1.1
```

Install `typos-cli` for spell and typo checking:

```sh
sudo apt install cargo
cargo install typos-cli --version 1.16.1
```

Install the Python modules used in the integration tests:

```sh
pip3 install setuptools wheel
pip3 install git+https://github.com/googleapis/storage-testbench
```

Add the pip directory to your PATH:

```sh
export PATH=$PATH:$HOME/.local/bin
```

### (Optional) Enable clang-based tooling in your IDE

Many IDEs use clang for smart code completion, refactoring, etc. To do this,
clang needs to know how to compile each file. This is commonly done by creating
a `compile_commands.json` file at the top of the repo, which CMake knows how to
create for us.

For the purposes of clang-tooling, it's best to build all the code and
dependencies from source so that clangd can find all the symbols and jump you to
the right spot in the source files. We prefer using `vcpkg` for this, so clone
the vcpkg repo if you don't already have it checked out somewhere.

```shell
VCPKG_ROOT=~/vcpkg  # We'll use this later
git clone https://github.com/microsoft/vcpkg "${VCPKG_ROOT}"
```

Next, use CMake with the
[CMAKE_EXPORT_COMPILE_COMMANDS](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html)
option to compile the `google-cloud-cpp` code, and tell it to use `vcpkg` to
build all the dependencies.

```shell
cmake -S . -B .build -G Ninja \
  --toolchain "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON \
  -DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .build
```

Finally, symlink the `.build/compile_commands.json` file into the top of the
repo.

```shell
ln -s .build/compile_commands.json .
```

Note: It's also possible to create `compile_commands.json` using `bazel`, but
it's not quite as easy. If you want to do that, take a look at
https://github.com/grailbio/bazel-compilation-database.

### Clone and compile `google-cloud-cpp`

You may need to clone and compile the code as described
[here](howto-guide-setup-cmake-environment.md).

Run the tests using:

```console
env -C cmake-out/home ctest --output-on-failure -LE integration-test
```

Run the Google Cloud Storage integration tests:

```console
env -C cmake-out/home \
    $PWD/google/cloud/storage/ci/run_integration_tests_emulator_cmake.sh .
```

Run the Google Cloud Bigtable integration tests:

```console
env -C cmake-out/home \
    $PWD/google/cloud/bigtable/ci/run_integration_tests_emulator_cmake.sh .
```

### Installing Docker

You may want to [install Docker](https://docs.docker.com/engine/installation/).
This will allow you to
[use the build scripts](howto-guide-running-ci-builds-locally.md) to test on
multiple platforms.

Once Docker is installed, to avoid needing to prepend `sudo` to Docker
invocations, add yourself to the Docker group:

```console
sudo usermod -aG docker $USER
```

## Windows

If you mainly use Windows as your development environment, you need to install a
number of tools. We use [Chocolatey](https://www.chocolatey.org) to drive the
installation, so you would need to install it first. This needs to be executed
in a `cmd.exe` shell, running as the `Administrator`:

```console
@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "iex (
(New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Then you can install the dependencies in the same shell:

```console
choco install -y visualstudio2019community visualstudio2019-workload-nativedesktop visualstudio2019buildtools
choco install -y git cmake ninja bazelisk
```

### Clone `google-cloud-cpp`

You may need to create a new key pair to connect to GitHub. Search the web for
how to do this. Then you can clone the code:

```console
> cd \Users\%USERNAME%
> git clone git@github.com:<GITHUB-USERNAME_HERE>/google-cloud-cpp.git
> cd google-cloud-cpp
```

### Use the CI scripts to compile `google-cloud-cpp`

You can use the CI driver scripts to compile the code. You need to load the MSVC
environment variables:

```console
> set MSVC_VERSION=2022
> call "c:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%\Community\VC\Auxiliary\Build\vcvars64.bat"
```

Or to setup for 32-bit builds:

```console
> set MSVC_VERSION=2022
> call "c:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%\Community\VC\Auxiliary\Build\vcvars32.bat"
```

Then run the CI scripts, for example, to compile and run the tests with CMake in
debug mode:

```console
> cd google-cloud-cpp
> powershell -exec bypass ci/kokoro/windows/build.ps1 cmake-debug
```

While to compile and run the tests with Bazel in debug mode you would use:

```console
> cd google-cloud-cpp
> powershell -exec bypass ci/kokoro/windows/build.ps1 bazel-debug
```

We used to have instructions to setup manual builds with CMake and Bazel on
Windows, but they quickly get out of date.

## macOS

> :warning: This is a work in progress. These instructions might be incomplete
> because we don't know how to create a "fresh" macOS install to verify we did
> not miss a step.

To build on macOS you will need the command-line development tools, and some
packages from [homebrew]. Start by installing the command-line development
tools:

```shell
sudo xcode-select --install
```

Verify that worked by checking your compiler:

```shell
c++ --version
# Expected output: Apple clang .*
```

Install [homebrew], their home page should have up to date instructions. Once
you have installed `homebrew`, use it to install some development tools:

```shell
brew update
brew install cmake bazel openssl git ninja
```

You may also want to install `ccache` to improve the rebuild times, and
`google-cloud-sdk` if you are planning to run the integration tests.

Then clone the repository:

```shell
cd $HOME
git clone git@github.com:<GITHUB-USERNAME_HERE>/google-cloud-cpp.git
cd google-cloud-cpp
```

### Manual builds with CMake

The [guide](/doc/contributor/howto-guide-setup-cmake-environment.md) generally
works for macOS too. We recommend using `Ninja` and `vcpkg` for development.
When using `vcpkg` you should disable the OpenSSL checks:

```shell
git clone -C $HOME https://github.com/microsoft/vcpkg.git
env VCPKG_ROOT=$HOME/vcpkg $HOME/vcpkg/bootstrap-vcpkg.sh
cmake -GNinja -S. -Bcmake-out/ \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF
cmake --build cmake-out
```

### Running the CI scripts

The CI scripts follow a similar pattern to the scripts for Linux and Windows:

```shell
./ci/kokoro/macos/build.sh bazel        # <-- Run the `bazel` CI build
./ci/kokoro/macos/build.sh cmake-vcpkg  # <-- Build with CMake
```

## Appendix: Linux VM on Google Compute Engine

From time to time you may want to setup a Linux VM in Google Compute Engine.
This might be useful to run performance tests in isolation, but "close" to the
service you are doing development for. The following instructions assume you
have already created a project:

```sh
PROJECT_ID=$(gcloud config get-value project)
# Or manually set it if you have not configured your default project:
```

Select a zone to run your VM:

```sh
gcloud compute zones list
$ ZONE=... # Pick a zone.
```

Select the name of the VM:

```sh
VM=... # e.g. VM=my-windows-devbox
```

Then create the virtual machine using:

```sh
IMAGE_PROJECT=ubuntu-os-cloud

IMAGE=$(gcloud compute images list \
    --project=${IMAGE_PROJECT} \
    --filter="family ~ ubuntu-2204" \
    --sort-by=~name \
    --format="value(name)" \
    --limit=1)

PROJECT_NUMBER=$(gcloud projects list \
    --filter="project_id=${PROJECT_ID}" \
    --format="value(project_number)" \
    --limit=1)

gcloud compute instances create ${VM} \
    --project=${PROJECT_ID} \
    --service-account=${PROJECT_NUMBER}-compute@developer.gserviceaccount.com \
    --create-disk=auto-delete=yes,boot=yes,device-name=${VM},image=projects/${IMAGE_PROJECT}/global/images/${IMAGE},mode=rw,size=2048,type=projects/${PROJECT_ID}/zones/${ZONE}/diskTypes/pd-balanced \
    --zone=${ZONE} \
    --machine-type=n2d-standard-128 \
    --network-interface=network-tier=PREMIUM,stack-type=IPV4_ONLY,subnet=default \
    --maintenance-policy=MIGRATE \
    --provisioning-model=STANDARD \
    --scopes=https://www.googleapis.com/auth/cloud-platform \
    --no-shielded-secure-boot \
    --shielded-vtpm \
    --shielded-integrity-monitoring \
    --labels=goog-ec-src=vm_add-gcloud \
    --reservation-affinity=any
```

Update the `ssh` configuration to include this new VM:

```sh
# You need to do this every time you add GCE instances.
gcloud compute config-ssh
```

```sh
# Googlers should do this once. It configures SSH to use a tunnel and to forward
# the authentication agent on all VMs in your project.
cat >>.ssh/config <__EOF__
Host *.${PROJECT_ID}
    ProxyCommand=corp-ssh-helper %h %p
    ForwardAgent yes
__EOF__
```

Then login using:

```sh
ssh -A ${VM}.${ZONE}.${PROJECT_ID}
```

## Appendix: Windows VM on Google Compute Engine

If you do not have a Windows workstation, but need a Windows development
environment to troubleshoot a test or build problem, it might be convenient to
create a Windows VM. The following commands assume you have already created a
project:

```console
$ PROJECT_ID=... # Set to your project id
```

Select a zone to run your VM:

```console
$ gcloud compute zones list
$ ZONE=... # Pick a zone close to where you are...
```

Select the name of the VM:

```console
$ VM=... # e.g. VM=my-windows-devbox
```

Then create the virtual machine using:

```console
$ IMAGE=$(gcloud compute images list \
    --sort-by=~name \
    --format="value(name)" \
    --limit=1)
$ PROJECT_NUMBER=$(gcloud projects list \
    --filter="project_id=${PROJECT_ID}" \
    --format="value(project_number)" \
    --limit=1)
$ gcloud compute --project "${PROJECT_ID}" instances create "${VM}" \
  --zone "${ZONE}" \
  --image "${IMAGE}" --image-project "windows-cloud" \
  --boot-disk-size "1024" --boot-disk-type "pd-standard" \
  --boot-disk-device-name "${VM}" \
  --service-account "${PROJECT_NUMBER}-compute@developer.gserviceaccount.com" \
  --machine-type "n1-standard-8" \
  --subnet "default" \
  --maintenance-policy "MIGRATE" \
  --scopes "https://www.googleapis.com/auth/bigtable.admin","https://www.googleapis.com/auth/bigtable.data","https://www.googleapis.com/auth/cloud-platform"
```

Reset the password for your account:

```console
$ gcloud compute --project "${PROJECT_ID}" reset-windows-password --zone "${ZONE}" "${VM}"
```

Save that password in some kind of password manager. Then connect to the VM
using your favorite RDP client. The Google Cloud Compute Engine
[documentation](https://cloud.google.com/compute/docs/quickstart-windows)
suggests some third-party clients that may be useful.

[appending-linux]: #appendix-linux-vm-on-google-compute-engine
[appending-windows]: #appendix-windows-vm-on-google-compute-engine
[homebrew]: https://brew.sh/
