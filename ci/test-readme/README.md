# Verification Scripts for the Build Instructions

The Dockerfiles in this directory serve as verification scripts for the
installation instructions in the top-level [README](../../README.md) and
[INSTALL](../../INSTALL.md) files.

The CI scripts and Dockerfiles are more complex than what most users care about,
they install development tools, such as code formatters, and drivers for the
integration tests. The instructions in the README and INSTALL files are intended
to be minimal and to the point.

Verifying these instructions manually is very tedious, instead, we use
Docker as a scripting language to execute these instructions, and then
automatically reformat the "script" as markdown.

## HOWTO: Generate the INSTALL.md file.

The INSTALL file is generated from these Dockerfiles, just run:

```bash
cd google-cloud-cpp
./ci/test-readme/generate-install.sh >INSTALL.md
```

and then create a pull request to merge your changes.

## HOWTO: Generate the README.md file.

The README file is partially generated from these Dockerfiles, just run:

```bash
cd google-cloud-cpp
./ci/test-readme/generate-readme.sh >temp.md
```

Then manually replace the section in the README.md file with the contents of
`temp.md` and create a pull request to merge your changes.

## HOWTO: Manually verify the instructions.

If you need to change the instructions then change the relevant Dockerfile and
execute (for example):

```bash
export NCPU=$(nproc)
cd google-cloud-cpp
sudo docker build --build-arg "NCPU=${NCPU}" -f ci/kokoro/readme/Dockerfile.ubuntu .
```

Recall that Docker caches intermediate results. If you are testing with very
active distributions (e.g. `openSUSE Tumbleweed`), where the base image changes
frequently, you may need to manually delete that base image from your cache to
get newer versions of said image.

Note that any change to the `google-cloud-cpp` contents, including changes to
the README, INSTALL, or any other documentation, starts the docker build almost
from the beginning. This is working as intended, we want these scripts to
simulate the experience of a user downloading `google-cloud-cpp` on a fresh
workstation.
