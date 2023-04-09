# Doxygen XML to DocFX for google-cloud-cpp

This directory contains a tool to convert Doxygen XML into DocFX YAML files.
This tool is used to generate the `google-cloud-cpp` reference documentation
when hosted at cloud.google.com.

This is not a general-purpose tool. It is not expected to work for other C++
projects. This tool is only used as part of the documentation pipeline for
`google-cloud-cpp`. As such, it is only tested on a limited number of platforms.

## End-to-End Testing

```
cd google-cloud-cpp
DOCKER_NETWORK=host ci/cloudbuild/build.sh -t publish-docs-pr
xsltproc build-out/fedora-37-cmake-publish-docs/cmake-out/google/cloud/xml/{combine.xslt,index.xml} >../common.doxygen.xml

git clone https://github.com/googleapis/doc-pipeline.git $HOME/doc-pipeline
cmake --build .build/ --target docfx/all \
  && rm -f $HOME/doc-pipeline/testdata/cpp/* \
  && env -C $HOME/doc-pipeline/testdata/cpp $PWD/.build/docfx/doxygen2docfx $HOME/common.doxygen.xml common 2.9.0

env -C $HOME/doc-pipeline \
    INPUT=testdata/cpp \
    TRAMPOLINE_BUILD_FILE=./generate.sh \
    TRAMPOLINE_IMAGE=gcr.io/cloud-devrel-kokoro-resources/docfx \
    TRAMPOLINE_DOCKERFILE=docfx/Dockerfile ci/trampoline_v2.sh
```
