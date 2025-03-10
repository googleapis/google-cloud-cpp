# How-to Guide: Update Conan Package

Updating the Conan packages is relatively involved and it typically takes weeks
for the PRs to go through reviews. Check with coryan@ or dbolduc@ before
embarking on this quest.

This package manager uses CMake, but ignores the library and target definitions
exported by our CMake files. It reimplements the library definitions and
dependencies in Python code. The recipes do this for every package (gRPC,
Protobuf, Boost, etc.), not just for `google-cloud-cpp`. For most packages,
where there are only a handful of libraries, this is trivial. But
`google-cloud-cpp` has about 240 libraries (including the `*_protos` libraries),
so we use a helper script to automatically generate the Python data structures
that describe the libraries in each of our versions.

This package manager requires patches to our code. These patches need to be
updated on each release. Package management systems tend to apply patches with
very strict settings, so even small changes around the patches break them.
Sometimes one can use `patch(1)` manually, with looser settings, and use that to
update the patches.

[PR#17988] is probably a good example of the changes you will need to make.

## Prepare your environment

Create a fork of
[conan-center-index](https://github.com/conan-io/conan-center-index.git)

Clone the fork:

```shell
git clone git@github.com:${GITHUB_USERNAME}/conan-center-index
cd conan-center-index
```

## Verify your environment is able to build the current Conan package

We will use `TAG=2.15.1` and `UTAG=2_15_1` and `LATEST=2.12.0` in these
examples. `LATEST` represents the latest version in Conan, and `TAG` represents
the version we want to add to Conan. It is common to skip several
`google-cloud-cpp` releases as it may take months to complete one update.

If you have not used Conan in a while, you may want to remove any stale
directories:

```shell
rm -fr ~/.conan2
rm -fr ~/.venv/conan2
```

Create an isolated Python environment and then install Conan and the hooks for
Conan package development:

```shell
python3 -m venv ~/.venv/conan2
# Activate the environment
. ~/.venv/conan2/bin/activate
# Install conan (it is a Python package)
pip install -U conan
# These hooks validate packages and create warnings
conan config install https://github.com/conan-io/hooks.git
conan config install https://github.com/conan-io/hooks.git -sf hooks -tf hooks
```

The Conan CI system compiles the code with GCC 11, so we will use the same:

```shell
sudo apt install -y gcc-11 g++-11
env CC=gcc-11 CXX=g++-11 conan profile detect --force
```

Test the existing recipe with the latest version. This may fail due to timeouts
reaching https://center.conan.io. Run the command agan if needed.

```shell
# In the conan-center-index workspace.
env CC=gcc-11 CXX=g++-11 conan create --build missing  --build-require  --version ${LATEST} recipes/google-cloud-cpp/2.x
```

## Update the package

Start by creating a git repository based on the release, for example:

```shell
curl -sSL https://github.com/googleapis/google-cloud-cpp/archive/v${TAG}.tar.gz | tar -C $HOME -zxf -
git -C $HOME/google-cloud-cpp-${TAG}/ init
git -C $HOME/google-cloud-cpp-${TAG}/ add .
git -C $HOME/google-cloud-cpp-${TAG}/ commit -q -m"Prepare for conan patches"
```

Apply the existing patch, manually fix any problems if this fails:

```shell
env -C $HOME/google-cloud-cpp-${TAG}/ patch -p1 \
    <recipes/google-cloud-cpp/2.x/patches/${LATEST}/001-use-conan-msvc-runtime.patch
```

Generate a new version of the patch:

```shell
mkdir -p recipes/google-cloud-cpp/2.x/patches/${TAG}
git -C $HOME/google-cloud-cpp-${TAG}/ diff \
    >recipes/google-cloud-cpp/2.x/patches/${TAG}/001-use-conan-msvc-runtime.patch
git -C $HOME/google-cloud-cpp-${TAG}/ commit -m"001-use-conan-msvc-runtime.patch" .
```

Create the support files:

```shell
recipes/google-cloud-cpp/2.x/extract_dependencies.py \
    --source-folder=$HOME/google-cloud-cpp-${TAG} \
    >recipes/google-cloud-cpp/2.x/components_${UTAG}.py
```

Manually edit these files:

```shell
recipes/google-cloud-cpp/config.yml
recipes/google-cloud-cpp/2.x/conandata.yml
recipes/google-cloud-cpp/2.x/conanfile.py
```

Test the recipe. This may fail due to timeouts reaching https://center.conan.io.
Run the command agan if needed.

```shell
env CC=gcc-11 CXX=g++-11 conan create --build missing  --build-require  --version ${TAG} recipes/google-cloud-cpp/2.x 2>&1 | tee create.log
```

Review [Conan's policy][conan-old-version-policy] with respect to old versions.
The policy only requires keeping 3 minor versions of the current major release
series. Take advantage of the policy to clean up old versions **and** the code
to support them.

For example, if you are updating Conan to support v2.19.0 and Conan already
supports v2.5.0, v2.12.0, v2.15.1, and v1.40.1 you can remove v2.5.0. You cannot
remove support for v1.40.1.

## Testing with Conan v1

The Conan team still supports Conan v1 and their CI system still requires the
recipes to pass with it. Most of the time there is no need to test Conan v1
locally. If your PR fails with a v1-specific error, consider running the test
locally:

```shell
python3 -m venv ~/.venv/conan1
. ~/.venv/conan1/bin/activate
pip install conan==1.62.0
# These hooks validate packages and create warnings
conan config install https://github.com/conan-io/hooks.git -sf hooks -tf hooks
conan config set hooks.attribute_checker
env CC=gcc-11 CXX=g++-11 conan create --build missing  --build-require  recipes/google-cloud-cpp/2.x google-cloud-cpp/${TAG}@user/channel 2>&1 | tee create_1.log
env CC=gcc-11 CXX=g++-11 conan test recipes/google-cloud-cpp/2.x/test_package google-cloud-cpp/${TAG}@user/channel 2>&1 | tee test_1.log
env CC=gcc-11 CXX=g++-11 conan test recipes/google-cloud-cpp/2.x/test_v1_package google-cloud-cpp/${TAG}@user/channel 2>&1 | tee test_1.log
```

## Troubleshooting dependency problems

Using `conan create` can be relatively slow. Sometimes you need to just
troubleshoot the component definitions. It is possible to skip the build step
and get a quicker edit+test cycle.

Put the package in
[editable mode](https://docs.conan.io/2/tutorial/developing_packages/editable_packages.html)

```shell
env CC=gcc-11 CXX=g++-11 conan editable add recipes/google-cloud-cpp/2.x
```

Get the source and build the package:

```shell
env CC=gcc-11 CXX=g++-11 conan source --version ${TAG} recipes/google-cloud-cpp/2.x
env CC=gcc-11 CXX=g++-11 conan build --version ${TAG} recipes/google-cloud-cpp/2.x 2>&1 | tee build.log
```

Run the `export-pkg` step:

```shell
env CC=gcc-11 CXX=g++-11 conan export-pkg --version ${TAG} recipes/google-cloud-cpp/2.x 2>&1 | tee export.log
```

If needed edit `conanfile.py` and repeat the export step as many times as
needed.

Remember to clear all the version of the package and test the final version:

```shell
conan remove -c google-cloud-cpp/*
conan editable remove recipes/google-cloud-cpp/2.x
env CC=gcc-11 CXX=g++-11 conan create --build missing  --build-require  --version ${TAG} recipes/google-cloud-cpp/2.x 2>&1 | tee create.log
```
