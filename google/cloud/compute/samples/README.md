# Running the Samples

Install Bazel using [these instructions][bazel-install]. If needed, install the
C++ toolchain for your platform too. Then compile the library samples using:

```console
bazel build //google/cloud/compute/samples:samples
```

On Windows you may need to add some flags to work around the filename length
limits.

> You must provide this option in **all** Bazel invocations shown below.

```console
mkdir C:\b
bazel --output_user_root=C:\b build //google/cloud/compute/samples:samples
```

You will need a Google Cloud Project with billing and the Compute Engine API
enabled. Please consult the Compute [quickstart guide][compute-quickstart-link]
for detailed instructions on how to enable billing for your project. Once your
project is properly configured you can run the samples using `bazel run`, for
example:

```console
bazel run //google/cloud/compute/samples:samples -- compute-disk-create-empty-disk PROJECT_ID ZONE DISK_NAME DISK_SIZE [DISK_LABELS]
bazel run //google/cloud/compute/samples:samples -- compute-disk-delete-disk PROJECT_ID ZONE DISK_NAME
```

Running the command without options will list the available samples:

```console
bazel run //google/cloud/compute/samples:samples
```

[bazel-install]: https://docs.bazel.build/versions/main/install.html
[compute-quickstart-link]: https://cloud.google.com/compute/docs/quickstart
