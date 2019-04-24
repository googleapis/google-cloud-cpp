# HOWTO: Setup your Development Environment

## Linux

Install the dependencies needed for your distribution. The top-level
[README](../README.md) file should list the minimal development tools necessary
to compile `google-cloud-cpp`. But for active development you may want to
install additional tools to run the unit and integration tests.

These instructions will describe how to install these tools for
Ubuntu 18.04 (Bionic Beaver). For other distributions you may consult the
Dockerfile used by the integration tests. For example,
[Dockerfile.ubuntu](../ci/travis/Dockerfile.ubuntu), or
[Dockerfile.fedora](../ci/travis/Dockerfile.fedora).

First, install the basic development tools:

```console
sudo apt update
sudo apt install -y build-essential cmake git gcc g++ cmake \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libssl-dev make \
        pkg-config tar wget zlib1g-dev
```

Then install `clang-6.0` and some additional Clang tools that we use to enforce
code style rules:

```console
sudo apt install -y clang-6.0 clang-tidy clang-format-7 clang-tools
sudo update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-6.0 100
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-7 100
sudo update-alternatives --install /usr/bin/scan-build scan-build /usr/bin/scan-build-6.0 100
```

Install buildifier tool, which we use to format `BUILD` files:

```console
sudo wget -q -O /usr/bin/buildifier https://github.com/bazelbuild/buildtools/releases/download/0.17.2/buildifier
sudo chmod 755 /usr/bin/buildifier
```

Install cmake_format to automatically format the CMake list files. We pin this
tool to a specific version because the formatting changes when the "latest"
version is updated, and we do not want the builds to break just
because some third party changed something.

```console
sudo apt install -y python python-pip
pip install --upgrade pip
pip install numpy cmake_format==0.4.0
```

Install the Python modules used in the integration tests:

```console
pip install flask httpbin gevent gunicorn crc32c --user
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
wget -q https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-233.0.0-linux-x86_64.tar.gz
sha256sum google-cloud-sdk-233.0.0-linux-x86_64.tar.gz | \
    grep -q '^a04ff6c4dcfc59889737810174b5d3c702f7a0a20e5ffcec3a5c3fccc59c3b7a '
echo $?
tar x -C $HOME -f google-cloud-sdk-233.0.0-linux-x86_64.tar.gz
$HOME/google-cloud-sdk/bin/gcloud --quiet components install cbt bigtable
```

### Clone and compile `google-cloud-cpp`

You may need to create a new key pair to connect to GitHub.  Search the web
for how to do this.  Then you can clone the code:

```console
cd $HOME
git clone git@github.com:<GITHUB-USERNAME_HERE>/google-cloud-cpp.git
cd google-cloud-cpp
```

And compile the code using:

```console
cmake -H. -Bcmake-out
cmake --build cmake-out -- -j $(nproc)
```

Run the tests using:

```console
env -C cmake-out ctest --output-on-failure
```

Run the Google Cloud Storage integration tests:

```console
env -C cmake-out $PWD/google/cloud/storage/ci/run_integration_tests.sh
```

Run the Google Cloud Bigtable integration tests:

```console
env -C cmake-out \
    CBT=$HOME/google-cloud-sdk/bin/cbt \
    CBT_EMULATOR=$HOME/google-cloud-sdk/platform/bigtable-emulator/cbtemulator \
    $PWD/google/cloud/bigtable/ci/run_integration_tests.sh
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
> choco install -y cmake git cmake.portable activeperl ninja golang yasm putty
> choco install -y visualstudio2017community visualstudio2017-workload-nativedesktop microsoft-build-tools
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

### Clone and compile `google-cloud-cpp`

You may need to create a new key pair to connect to GitHub.  Search the web
for how to do this.  Then you can clone the code:

```console
> cd \Users\%USERNAME%
> git clone git@github.com:<GITHUB-USERNAME_HERE>/google-cloud-cpp.git
> cd google-cloud-cpp
```

And compile the code using:

```console
> call "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
> cmake -GNinja -H. -Bcmake-out
   -DCMAKE_BUILD_TYPE=Debug
   -DCMAKE_TOOLCHAIN_FILE=C:\Users\%USERNAME%\vcpkg\scripts\buildsystems\vcpkg.cmake
   -DVCPKG_TARGET_TRIPLET=x64-windows-static
   -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package
> cmake --build cmake-out
```

Run the tests using:

```console
> cd cmake-out
> ctest --output-on-failure
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
