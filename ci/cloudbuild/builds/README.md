This directory contains short scripts with the commands for a particular "build
name". These scripts are intended to be executed by the
`ci/cloudbuild/build.sh` driver script. They may be run within a Cloud Build
docker image, or within another user-provided docker image, or even natively on
the users local machine.

Adding a new build is as easy as adding a new script in this directory.
