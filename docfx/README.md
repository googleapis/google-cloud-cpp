# Doxygen XML to DocFX for google-cloud-cpp

This directory contains a tool to convert Doxygen XML into DocFX YAML files.
This tool is used to generate the `google-cloud-cpp` reference documentation
when hosted at cloud.google.com.

This is not a general-purpose tool. It is not expected to work for other C++
projects. This tool is only used as part of the documentation pipeline for
`google-cloud-cpp`. As such, it is only tested on a limited number of platforms.

## End-to-End Testing

This assumes you are building with CMake. More details on CMake configuration in
the [How-to Guide](/doc/contributor/howto-guide-setup-environment.md).

### Create the input files for the common library

```
cd google-cloud-cpp
ci/cloudbuild/build.sh -t publish-docs-pr
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
cmake -GNinja -S . -B build-out/.docfx -DGOOGLE_CLOUD_CPP_INTERNAL_DOCFX=ON \
    --toolchain $HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
PUBLISH=$PWD/build-out/fedora-latest-publish-docs/publish-docs/__default__/cmake-out/google/cloud
cmake --build build-out/.docfx --target docfx/all \
  && rm -f $HOME/doc-pipeline/testdata/cpp/* \
  && env -C $HOME/doc-pipeline/testdata/cpp $PWD/build-out/.docfx/docfx/doxygen2docfx $PUBLISH/xml/cloud.doxygen.xml cloud 2.9.0
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

### Testing a library other than the common library

Run the previous steps and then:

```
cp $HOME/doc-pipeline/site/xrefmap.yml build-out
cmake --build .build/ --target docfx/all \
  && rm -f $HOME/doc-pipeline/testdata/cpp/* \
  && env -C $HOME/doc-pipeline/testdata/cpp $PWD/.build/docfx/doxygen2docfx $PUBLISH/secretmanager/xml/secretmanager.doxygen.xml secretmanager 2.9.0
sed '1,2d' build-out/xrefmap.yml >>$HOME/doc-pipeline/testdata/cpp/toc.yml
sed -i '6d' $HOME/doc-pipeline/testdata/cpp/docs.metadata.json
```

```
env -C $HOME/doc-pipeline \
    INPUT=testdata/cpp \
    TRAMPOLINE_BUILD_FILE=./generate.sh \
    TRAMPOLINE_IMAGE=gcr.io/cloud-devrel-kokoro-resources/docfx \
    TRAMPOLINE_DOCKERFILE=docfx/Dockerfile ci/trampoline_v2.sh
```

## Detecting Doxygen schema changes

From time to time we update Doxygen and that may result in schema changes.
Detecting these changes may be useful to design the corresponding changes to the
`doxygen2docfx` tool.

The schema definition can be found in the [Doxygen GitHub Repository]. Look for
[xml/compound.xsd](https://github.com/doxygen/doxygen/blob/master/templates/xml/compound.xsd)

[doxygen github repository]: https://github.com/doxygen/doxygen
