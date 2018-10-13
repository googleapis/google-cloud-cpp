# HOWTO: Setup your Development Environment

## Linux

Install the dependencies needed for your distribution.  The best documentation
are the Docker files used for the continuous integration builds.  For example,
if you are planning to develop on Ubuntu open
[Dockerfile.ubuntu](../ci/travis/Dockerfile.ubuntu), there you will find the
tools to install:

```Dockerfile
RUN apt-get update && apt-get install -y \
  automake \
  build-essential \
  clang \
  ... # More stuff omitted.
```

Run the same commands as `root` in your workstation and your environment will
be ready.

### Installing Docker

You may want to [install Docker](https://docs.docker.com/engine/installation/),
this will allow you to use the build scripts to test on multiple platforms,
as described in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Windows

If you mainly use Windows as your development environment, you need to install
a number of tools.  We use [Chocolatey](https://www.chocolatey.com) to drive the
installation, so you would need to install it first.  This needs to be executed
in a `cmd.exe` shell, running as the `Administrator`:

```commandline
C:\...> @"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "iex (
(New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Then you can install the dependencies in the same shell:
```commandline
C:\...> choco install -y cmake git cmake.portable activeperl ninja golang yasm putty
C:\...> choco install -y visualstudio2017community
C:\...> choco install -y visualstudio2017-workload-nativedesktop
C:\...> choco install -y microsoft-build-tools
```

The previous installation should create a
`Developer Command Prompt for VS 2017` entry in your "Windows" menu, use that
entry to create a new shell.
In that shell, install `vcpkg` the Microsoft-supported ports for many Open
Source projects:

```commandline
C:\...> cd \Users\%USERNAME%
C:\...> git clone --depth 10 https://github.com/Microsoft/vcpkg.git
C:\...> cd vcpkg
C:\...> .\bootstrap-vcpkg.bat
C:\...> .\vcpkg install grpc abseil
C:\...> .\vcpkg integrate install
```

You may need to create a new key pair to connect to GitHub.  Search the web
for how to do this.  Then you can clone the code:

```commandline
C:\...> cd \Users\%USERNAME%\source
C:\...> git clone git@github.com:<GITHUB-USERNAME_HERE>/google-cloud-cpp.git
C:\...> cd google-cloud-cpp
C:\...> git submodule update --init
```

And compile the code using:

```commandline
C:\...> mkdir .build
C:\...> cd .build
C:\...> cmake -DCMAKE_TOOLCHAIN_FILE=C:/Users/%USERNAME%/vcpkg/scripts/buildsystems/vcpkg.cmake -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package ..
C:\...> cmake --build . -- /m
```

Run the tests using:

```commandline
C:\...> ctest -C Debug .
```

### Creating a Windows VM using Google Compute Engine

If you do not have a Windows workstation, but need a Windows development
environment to troubleshoot a test or build problem, it might be convenient to
create a Windows VM. The following commands assume you have already created a
project:

```commandline
$ PROJECT_ID=... # Set to your project id
```

Select a zone to run your VM:

```commandline
$ gcloud compute zones list
$ ZONE=... # Pick a zone close to where you are...
```

Select the name of the VM:

```commandline
$ VM=... # e.g. VM=my-windows-devbox
```

Then create the virtual machine using:

```commandline
$ IMAGE=$(gcloud compute images list --filter="family=('windows-2016')" --sort-by=name | awk '{print $1}' | tail -1)
$ PROJECT_NUMBER=$(gcloud projects list --filter="project_id=${PROJECT_ID}" | awk '{print $3}' | tail -1)
$ gcloud compute --project "${PROJECT_ID}" instances create "${VM}" \
  --zone "${ZONE}" \
  --image "${IMAGE}" --image-project "windows-cloud" \
  --boot-disk-size "128" --boot-disk-type "pd-standard" --boot-disk-device-name "${VM}" \
  --service-account "${PROJECT_NUMBER}-compute@developer.gserviceaccount.com" \
  --machine-type "n1-standard-8" \
  --subnet "default" \
  --maintenance-policy "MIGRATE" \
  --scopes "https://www.googleapis.com/auth/bigtable.admin","https://www.googleapis.com/auth/bigtable.data","https://www.googleapis.com/auth/cloud-platform"
```

Reset the password for your account:

```commandline
$ gcloud compute --project "${PROJECT_ID}" reset-windows-password --zone "${ZONE}" "${VM}"
```

Save that password in some kind of password manager.  Then connect to the VM
using your favorite RDP client.  The Google Cloud Compute Engine
[documentation](https://cloud.google.com/compute/docs/quickstart-windows)
suggests some third-party clients that may be useful.

### Connecting to GitHub with PuTTY

This short recipe is offered to setup your SSH keys quickly using PuTTY.  If
you prefer another SSH client for Windows, please search the Internet for a
tutorial on how to configure it.  Generate a private/public key pair with
`puttygen`:

```commandline
C:\...> puttygen
```

Then store the public key in your
[GitHub Settings](https://github.com/settings/keys).

Once you have generated the public/private key pair, start the SSH agent in the
background:

```commandline
C:\...> pageant
```

and use the menu to load the private key you generated above. Test the keys
with:

```commandline
C:\...> plink -T git@github.com
```

and do not forget to setup the `GIT_SSH` environment variable:

```commandline
C:\...> set GIT_SSH=plink
C:\...> git clone git@github.com:<GITHUB-USERNAME-HERE>/google-cloud-cpp.git
```
