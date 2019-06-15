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
cd google-cloud-cpp
sudo docker build -f ci/test-readme/Dockerfile.ubuntu .
```

Recall that Docker caches intermediate results. If you are testing with very
active distributions (e.g. `opensuse:tumbleweed`), where the base image changes
frequently, you may need to manually delete that base image from your cache to
get newer versions of said image.

Note that any change to the `google-cloud-cpp` contents, including changes to
the README, INSTALL, or any other documentation, starts the docker build almost
from the beginning. This is working as intended, we want these scripts to
simulate the experience of a user downloading `google-cloud-cpp` on a fresh
workstation.

## HOWTO: Setup the Google Container Registry cache.

These builds fail from time to time because the services to download packages
have outages (more often than you would expect). These outages can last hours,
the usual approach of "try N times with a backoff" do not work. To minimize the
impact of these outages, we cache these images in Google Container Registry,
and use the cache for all pull requests. Only pushes to master update the cache,
and only if rebuilding the cache succeeded.

Pick a project that will host the container images cache:

```console
$ PROJECT_ID=... # Most likely an existing project for builds.
```

We need to grant the service account full storage admin access to the bucket
that will contain the containers. To this end we need to create the
[bucket first](https://cloud.google.com/container-registry/docs/access-control):

```console
$ gcloud auth configure-docker
$ docker pull hello-world:latest
$ docker tag hello-world:latest gcr.io/${PROJECT_ID}/hello-world:latest
$ docker push gcr.io/${PROJECT_ID}/hello-world:latest
```

Create a service account to access the Google Container Registry:

```console
$ SA_NAME=kokoro-gcr-updater-sa
```

Then create the service account:

```console
$ gcloud iam service-accounts create --project ${PROJECT_ID} ${SA_NAME} \
    --display-name="Allows Kokoro builds to access GCR images."
$ FULL_SA_NAME="${SA_NAME}@${PROJECT_ID}.iam.gserviceaccount.com"
```

Grant the service account `roles/storage.admin` permissions on the container
registry bucket.

```console
$ gsutil iam ch serviceAccount:${FULL_SA_NAME}:roles/storage.admin \
    gs://artifacts.${PROJECT_ID}.appspot.com/ 
```

Create new keys for this service account and download then to a temporary place:

```console
$ gcloud iam service-accounts keys create /dev/shm/gcr-service-account.json \
      --iam-account "${FULL_SA_NAME}"
```

Create a configuration file for the Kokoro script:

```bash
$ echo "PROJECT_ID=${PROJECT_ID}" >/dev/shm/gcr-configuration.sh
```

Copy the service account keys and configuration file to location where we keep
Kokoro secrets (see .