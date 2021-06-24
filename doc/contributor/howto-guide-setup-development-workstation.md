# How-to Guide: Setup your Development Environment

This guide is intended for contributors to the `google-cloud-cpp` libraries.
This will walk you through the steps necessary to setup your development
workstation to compile the code, run the unit and integration tests, and send
contributions to the project.

* Packaging maintainers or developers that prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](/README.md#quickstart) for the
  library or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should consult the
  [building and installing](/README.md#building-and-installing) section of the
  top-level README file.
* Contributors and developers **to** `google-cloud-cpp`, this is the right
  document

## Table of Contents

* [Linux](#linux)
* [Windows](#windows)
* [macOS](#macos)
* [Appendix: Linux VM on Google Compute Engine][appending-linux]
* [Appendix: Windows VM on Google Compute Engine][appending-windows]

[appending-linux]: #appendix-linux-vm-on-google-compute-engine
[appending-windows]: #appendix-windows-vm-on-google-compute-engine

## Linux

Install the dependencies needed for your distribution. The top-level
[README](/README.md) file lists the minimal development tools necessary
to compile `google-cloud-cpp`. But for active development you may want to
install additional tools to run the unit and integration tests.

These instructions will describe how to install these tools for Ubuntu 18.04
(Bionic Beaver). For other distributions you may consult the Dockerfiles in
[ci/cloudbuild/dockerfiles/](https://github.com/googleapis/google-cloud-cpp/tree/main/ci/cloudbuild/dockerfiles)
If you use a different distribution, you will need to use the corresponding
package manager (`dnf`, `zypper`, `apk`, etc.) and find the corresponding
package names.

First, install the basic development tools:

```console
sudo apt update
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libbenchmark-dev libcurl4-openssl-dev libssl-dev \
        make npm pkg-config tar wget zlib1g-dev
```

Then install `clang-10` and some additional Clang tools that we use to enforce
code style rules:

```console
sudo apt install -y clang-10 clang-tidy-10 clang-format-10 clang-tools-10
sudo update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-10 100
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-10 100
```

Install buildifier tool, which we use to format `BUILD` files:

```console
sudo wget -q -O /usr/bin/buildifier https://github.com/bazelbuild/buildtools/releases/download/0.29.0/buildifier
sudo chmod 755 /usr/bin/buildifier
```

Install shfmt tool, which we use to format our shell scripts:

```console
sudo curl -L -o /usr/local/bin/shfmt \
    "https://github.com/mvdan/sh/releases/download/v3.1.0/shfmt_v3.1.0_linux_amd64"
sudo chmod 755 /usr/local/bin/shfmt
```

Install `cmake_format` to automatically format the CMake list files. We pin this
tool to a specific version because the formatting changes when the "latest"
version is updated, and we do not want the builds to break just
because some third party changed something.

```console
sudo apt install -y python3 python3-pip
pip3 install --upgrade pip
pip3 install cmake_format==0.6.8
```

Install black, which we use to format our Python scripts:

```console
pip3 install black==19.3b0
```

Install cspell for spell checking.

```console
sudo npm install -g cspell
```

Install the Python modules used in the integration tests:

```console
pip3 install setuptools wheel
pip3 install git+git://github.com/googleapis/python-storage@8cf6c62a96ba3fff7e5028d931231e28e5029f1c
pip3 install flask==1.1.2 httpbin==0.7.0 scalpl==0.4.0 \
    crc32c==2.1 gunicorn==20.0.4
```

Add the pip directory to your PATH:

```console
export PATH=$PATH:$HOME/.local/bin
```

You need to install the Google Cloud SDK. These instructions work for a GCE
VM, but you may need to adapt them for your environment. Check the instructions
on the [Google Cloud SDK website](https://cloud.google.com/sdk/) for
alternatives.

```console
./ci/install-cloud-sdk.sh
```

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
This will allow you to [use the build scripts](howto-guide-running-ci-builds-locally.md)
to test on multiple platforms.

Once Docker is installed, to avoid needing to prepend `sudo` to Docker
invocations, add yourself to the Docker group:

```console
sudo usermod -aG docker $USER
```

## Windows

If you mainly use Windows as your development environment, you need to install
a number of tools.  We use [Chocolatey](https://www.chocolatey.org) to drive the
installation, so you would need to install it first.  This needs to be executed
in a `cmd.exe` shell, running as the `Administrator`:

```console
> @"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "iex (
(New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Then you can install the dependencies in the same shell:
```console
> choco install -y cmake git cmake.portable activeperl ninja golang yasm putty msys2 bazel
> choco install -y visualstudio2019community visualstudio2019-workload-nativedesktop microsoft-build-tools
```

### Connecting to GitHub with PuTTY

This short recipe is offered to setup your SSH keys quickly using PuTTY.  If
you prefer another SSH client for Windows, please search the Internet for a
tutorial on how to configure it.

First, generate a private/public key pair with `puttygen`:

```console
> puttygen
```

Then store the public key in your
[GitHub Settings](https://github.com/settings/keys).

Once you have generated the public/private key pair, start the SSH agent in the
background:

```console
> pageant
```

and use the menu to load the private key you generated above. Test the keys
with:

```console
> plink -T git@github.com
```

and do not forget to setup the `GIT_SSH` environment variable:

```console
> set GIT_SSH=plink
```

### Clone `google-cloud-cpp`

You may need to create a new key pair to connect to GitHub.  Search the web
for how to do this.  Then you can clone the code:

```console
> cd \Users\%USERNAME%
> git clone git@github.com:<GITHUB-USERNAME_HERE>/google-cloud-cpp.git
> cd google-cloud-cpp
```

### Use the CI scripts to compile `google-cloud-cpp`

You can use the CI driver scripts to compile the code. You need to load the
MSVC environment variables:

```console
> set MSVC_VERSION=2019
> call "c:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%\Community\VC\Auxiliary\Build\vcvars64.bat"
```

Or to setup for 32-bit builds:

```console
> set MSVC_VERSION=2019
> call "c:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%\Community\VC\Auxiliary\Build\vcvars32.bat"
```

Then run the CI scripts, for example, to compile and run the tests with CMake
in debug mode:

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

> :warning: this is work in progress, these instructions might be incomplete
> because we don't know how to create a "fresh" macOS install to verify we did
> not miss a step.

[homebrew]: https://brew.sh/

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

### Running the CI scripts

The CI scripts follow a similar pattern to the scripts for Linux and Windows:

```shell
./ci/kokoro/macos/build.sh bazel   # <-- Run the `bazel` CI build
./ci/kokoro/macos/build.sh cmake-super  # <-- Build with CMake
```

### Manual builds with CMake

The [guide](/doc/contributor/howto-guide-setup-cmake-environment.md) generally
works for macOS too. The only difference is the configuration for OpenSSL. The
native OpenSSL library on macOS does not work, but the one distributed by
`homebrew` does. You **must** set the `OPENSSL_ROOT_DIR` environment variable
before configuring CMake, so CMake can use this alternative version. You cannot
just pass this as a `-D` option to CMake, because the value must recurse to all
the external projects in the super build.

```shell
export OPENSSL_ROOT_DIR=/usr/local/opt/openssl
cmake -Hsuper -Bcmake-out/si \
    -DGOOGLE_CLOUD_CPP_EXTERNAL_PREFIX="${HOME}/local-cpp" -GNinja
cmake --build cmake-out/si --target project-dependencies

cmake -H. -Bcmake-out/home -DCMAKE_PREFIX_PATH="${HOME}/local-cpp" -GNinja
cmake --build cmake-out/home
```

## Appendix: Linux VM on Google Compute Engine

From time to time you may want to setup a Linux VM in Google Compute Engine.
This might be useful to run performance tests in isolation, but "close" to the
service you are doing development for. The following instructions assume you
have already created a project:

```console
$ PROJECT_ID=$(gcloud config get-value project)
# Or manually set it if you have not configured your default project:
```

Select a zone to run your VM:

```console
$ gcloud compute zones list
$ ZONE=... # Pick a zone.
```

Select the name of the VM:

```console
$ VM=... # e.g. VM=my-windows-devbox
```

Then create the virtual machine using:

```console
# Googlers should consult go/drawfork before selecting an image.
$ IMAGE_PROJECT=ubuntu-os-cloud
$ IMAGE=$(gcloud compute images list \
    --project=${IMAGE_PROJECT} \
    --filter="family ~ ubuntu-1804" \
    --sort-by=~name \
    --format="value(name)" \
    --limit=1)
$ PROJECT_NUMBER=$(gcloud projects list \
    --filter="project_id=${PROJECT_ID}" \
    --format="value(project_number)" \
    --limit=1)
$ gcloud compute --project "${PROJECT_ID}" instances create "${VM}" \
  --zone "${ZONE}" \
  --image "${IMAGE}" \
  --image-project "${IMAGE_PROJECT}" \
  --boot-disk-size "1024" --boot-disk-type "pd-standard" \
  --boot-disk-device-name "${VM}" \
  --service-account "${PROJECT_NUMBER}-compute@developer.gserviceaccount.com" \
  --machine-type "n1-standard-8" \
  --subnet "default" \
  --maintenance-policy "MIGRATE" \
  --scopes "https://www.googleapis.com/auth/bigtable.admin","https://www.googleapis.com/auth/bigtable.data","https://www.googleapis.com/auth/cloud-platform"
```

To login to this image use:

```console
$ gcloud compute ssh --ssh-flag=-A --zone=${ZONE} ${VM}
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

Save that password in some kind of password manager.  Then connect to the VM
using your favorite RDP client.  The Google Cloud Compute Engine
[documentation](https://cloud.google.com/compute/docs/quickstart-windows)
suggests some third-party clients that may be useful.
