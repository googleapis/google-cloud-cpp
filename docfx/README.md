# Doxygen XML to DocFX for google-cloud-cpp

This directory contains a tool to convert Doxygen XML into DocFX YAML files.
This tool is used to generate the `google-cloud-cpp` reference documentation
when hosted at cloud.google.com.

This is not a general-purpose tool. It is not expected to work for other C++
projects. This tool is only used as part of the documentation pipeline for
`google-cloud-cpp`. As such, it is only tested on a limited number of platforms.

## End-to-End Testing

This assumes you are building with CMake. More details on CMake configuration
in the [How-to Guide](/doc/contributor/howto-guide-setup-environment.md).

### Create the input files for the common library

```
cd google-cloud-cpp
DOCKER_NETWORK=host ci/cloudbuild/build.sh -t publish-docs-pr
xsltproc build-out/fedora-37-cmake-publish-docs/cmake-out/google/cloud/xml/{combine.xslt,index.xml} >../common.doxygen.xml
```

### Clone Google's tools to process DocFX

```
cd google-cloud-cpp
git clone https://github.com/googleapis/doc-pipeline.git $HOME/doc-pipeline
```

### Clone vcpkg

```
cd google-cloud-cpp
git clone https://github.com/microsoft/vcpkg.git $HOME/vcpkg
```

### Generate the DoxFX YAML from the Doxygen input

```
cd google-cloud-cpp
cmake -S . -B .build -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .build/ --target docfx/all \
  && rm -f $HOME/doc-pipeline/testdata/cpp/* \
  && env -C $HOME/doc-pipeline/testdata/cpp $PWD/.build/docfx/doxygen2docfx $HOME/common.doxygen.xml common 2.9.0
```

### Run the DocFX pipeline over the generated files

```
env -C $HOME/doc-pipeline \
    INPUT=testdata/cpp \
    TRAMPOLINE_BUILD_FILE=./generate.sh \
    TRAMPOLINE_IMAGE=gcr.io/cloud-devrel-kokoro-resources/docfx \
    TRAMPOLINE_DOCKERFILE=docfx/Dockerfile ci/trampoline_v2.sh
```

### Examine the HTML-ish output

The output is HTML with templates and embedded markdown:

```
ls -l $HOME/doc-pipeline/site
less $HOME/doc-pipeline/site/namespacegoogle.html
less $HOME/doc-pipeline/site/indexpage.html
```

### Testing with one or all Libraries

This is a bit slow, but runs all the libraries through the docfx tool. First
prepare the workspace for the `publish-docs` build:

```
ci/cloudbuild/build.sh -t publish-docs-pr
```

Then invoke the docfx tool in one library, for example:

```
ci/cloudbuild/build.sh -t publish-docs-pr  --docker-shell
# Run this inside the docker shell
cmake --build cmake-out --target storage-docfx
```

Or to generate the documents for all libraries:

```
ci/cloudbuild/build.sh -t publish-docs-pr  --docker-shell
# Run this inside the docker shell
cmake --build cmake-out --target all-docfx
```
