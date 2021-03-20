This directory contains the files for starting and configuring Google Cloud
Build. The `cloudbuild.yaml` file is the main config file for cloud build.
Cloud builds may be managed from the command line with the `gcloud builds`
command. To make this process easier, the `build.sh` script can be invoked by
users to trigger the builds specified in the `builds/` subdirectory. The
`build.sh` script can run these builds on the user's local machine, or it can
submit the builds to Cloud Build if a distro is specified with the
`-d|--distro` flag. Distros are defined by the existence of a
`Dockerfile.<distro>` file.

## Examples

The following command runs the `asan` build on the user's local machine.
```
$ ci/cloudbuild/build.sh asan
$ ci/cloudbuild/build.sh -d local asan  # equivalent to above
```

The following command runs the `cmake+clang` build on the in Cloud Build using
the `Dockerfile.fedora` image.
```
$ ci/cloudbuild/build.sh -d fedora cmake+clang
```
