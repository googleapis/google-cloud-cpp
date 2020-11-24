# HOWTO: Setup your Development Environment

## Linux

Install the dependencies needed for your distribution. The top-level
[README](../README.md) file should list the minimal development tools necessary
to compile `google-cloud-cpp`. But for active development you may want to
install additional tools to run the unit and integration tests.

These instructions will describe how to install these tools for Ubuntu 18.04
(Bionic Beaver). For other distributions you may consult the Dockerfile used by
the integration tests. We use
[Dockerfile.fedora-install](../ci/kokoro/docker/Dockerfile.fedora-install) to
to enforce formatting for our builds. If you use a different distribution, you
will need to use the corresponding package manager (`dnf`, `zypper`, `apk`,
etc.) and find the corresponding package names.

First, install the basic development tools:

```console
sudo apt update
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libbenchmark-dev libcurl4-openssl-dev libssl-dev \
        make pkg-config tar wget zlib1g-dev
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
curl -L -o /usr/local/bin/shfmt \
    "https://github.com/mvdan/sh/releases/download/v3.1.0/shfmt_v3.1.0_linux_amd64"
chmod 755 /usr/local/bin/shfmt
```

Install `cmake_format` to automatically format the CMake list files. We pin this
tool to a specific version because the formatting changes when the "latest"
version is updated, and we do not want the builds to break just
because some third party changed something.

```console
sudo apt install -y python3 python3-pip
pip3 install --upgrade pip3
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
on the
[Google Cloud SDK website](https://cloud.google.com/sdk/) for alternatives.

```console
./ci/install-cloud-sdk.sh
```

### Clone and compile `google-cloud-cpp`

You may need to clone and compile the code as described [here](setup-cmake-environment.md)

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

You may want to [install Docker](https://docs.docker.com/engine/installation/),
this will allow you to use the build scripts to test on multiple platforms,
as described in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Windows

If you mainly use Windows as your development environment, you need to install
a number of tools.  We use [Chocolatey](https://www.chocolatey.com) to drive the
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

### Compile `google-cloud-cpp` using cmake and vcpkg

### Download and compile `vcpkg`

The previous installation should create a
`Developer Command Prompt for VS 2017` entry in your "Windows" menu, use that
entry to create a new shell.
In that shell, install `vcpkg` the Microsoft-supported ports for many Open
Source projects:

```console
> cd \Users\%USERNAME%
> git clone --depth 10 https://github.com/Microsoft/vcpkg.git
> cd vcpkg
> .\bootstrap-vcpkg.bat
```

You can get `vcpkg` to compile all the dependencies for `google-cloud-cpp` by
installing `google-cloud-cpp` itself:

```console
> vcpkg.exe install google-cloud-cpp:x64-windows-static
> vcpkg.exe integrate install
```

Compile the code using:

```console
> cd \Users\%USERNAME%\google-cloud-cpp
> call "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
> cmake -GNinja -H. -Bcmake-out
   -DCMAKE_BUILD_TYPE=Debug
   -DCMAKE_TOOLCHAIN_FILE=C:\Users\%USERNAME%\vcpkg\scripts\buildsystems\vcpkg.cmake
   -DVCPKG_TARGET_TRIPLET=x64-windows-static
> cmake --build cmake-out
```

Run the tests using:

```console
> cd cmake-out
> ctest --output-on-failure
```

### Compile `google-cloud-cpp` using bazel

Due to Windows command line length limits, create an abbreviated output directory:
```console
mkdir c:\b
```

Compile the code:
```console
cd \Users\%USERNAME%\google-cloud-cpp
bazel --output_user_root="c:\b" build //google/cloud/...:all
```

Run all the tests:
```console
bazel --output_user_root="c:\b" test //google/cloud/...:all
```

## Appendix: Creating a Linux VM using Google Compute Engine

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

## Appendix: Creating a Windows VM using Google Compute Engine

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
