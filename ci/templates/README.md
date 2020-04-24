# Helper scripts to generate CI build scripts

The Google Cloud C++ projects largely share their CI scripts, but there are some
differences that cannot be (easily) handled by configuration files. This
directory contains helper scripts to generate the scripts or configuration
files that cannot be refactored in any other form.

To use these scripts use:

```bash
./generate-kokoro-install.sh ${MY_CLONE_DIR}
```

where `MY_CLONE_DIR` is the location of a clone for one of the Google Cloud C++
projects.

For example:

```bash
git clone git@github.com:$GIT_USER_NAME/google-cloud-cpp-common
git clone git@github.com:$GIT_USER_NAME/google-cloud-cpp-spanner
cd google-cloud-cpp-common
./ci/templates/generate-kokoro-install.sh ../google-cloud-cpp-spanner
```
