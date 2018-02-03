# Reproduce multi-threaded client crash in gRPC.

These programs were used to reproduce the crash described in
[#211](https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues/211).
These programs can be compiled with the grpc submodule included in the overall
project, or you can treat this directory as a top-level CMake project.

### Compiling as a top-level project.

First clone the version of grpc you are interested in:

```console
$ git clone https://github.com/grpc/grpc.git ext/grpc
# optionally: (cd extp/grpc ; git checkout branch/sha-1/etc )
```

Then compile as usual:

```console
mkdir .build
cd .build
cmake ..
```

### Running the test

Run the server on a port of your choosing, with enough threads to reproduce the
problem quickly, for example, use 128 threads on port 9876:

```console
./server 9876 128
```

Then run the client against this server for a few hours:

```console
./client localhost:9876 100 600
```

It may take several hours for the client to crash with a segmentation violation,
be patient.

