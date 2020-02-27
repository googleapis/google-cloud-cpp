# Running the Samples

Install Bazel using [these instructions][bazel-install]. If needed, install the
C++ toolchain for your platform too. Then compile the library samples using:

```console
bazel build //google/cloud/spanner/samples:samples
```

On Windows you may need to add some flags to workaround the filename length
limits.

> You must provide this option in **all** Bazel invocations shown below.

```console
mkdir C:\b
bazel --output_user_root=C:\b build //google/cloud/spanner/samples:samples
```

If you are using a Bazel version before 2.2.0 you may need to run the following command
to workaround [bazelbuild/bazel#10621](https://github.com/bazelbuild/bazel/issues/10621).

```console
bazel --output_user_root=C:\b test //google/cloud/spanner/samples:samples
```

On Windows and macOS gRPC [requires][grpc-roots-pem-bug] an environment variable
to find the root of trust for SSL. On macOS use:

```console
curl -Lo roots.pem https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"
```

While on Windows use:

```console
@powershell -NoProfile -ExecutionPolicy unrestricted -Command ^
    (new-object System.Net.WebClient).Downloadfile( ^
        'https://raw.githubusercontent.com/grpc/grpc/master/etc/roots.pem', ^
        'roots.pem')
set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=%cd%\roots.pem
```

You will need a Google Cloud Project with billing and the spanner API enabled.
Please consult the Spanner [quickstart guide][spanner-quickstart-link] for
detailed instructions on how to enable billing for your project.
Once your project is properly configured you can run the samples using
`bazel run`, for example:

```console
bazel run //google/cloud/spanner/samples:samples -- create-instance [PROJECT ID] [SPANNER INSTANCE] "My Test Instance"
bazel run //google/cloud/spanner/samples:samples -- create-database [PROJECT ID] [SPANNER INSTANCE] [DATABASE ID]
```

Running the command without options will list the available samples:

```console
bazel run //google/cloud/spanner/samples:samples
```

[bazel-install]: https://docs.bazel.build/versions/master/install.html
[spanner-quickstart-link]: https://cloud.google.com/spanner/docs/quickstart-console
[grpc-roots-pem-bug]: https://github.com/grpc/grpc/issues/16571
