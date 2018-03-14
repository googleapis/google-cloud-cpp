# Reproduce multi-threaded client crash in gRPC.

These programs were used to reproduce the crash described in
[#230](https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues/230).
These programs can be compiled with the grpc submodule included in the overall
project, or you can treat this directory as a top-level CMake project.

### Compiling as a top-level project.

First clone the version of gRPC you are interested in:

```console
git clone -b v1.9.x https://github.com/grpc/grpc.git ext/grpc
# optionally: (cd extp/grpc ; git checkout branch/sha-1/etc )
```

Get the submodules for gRPC:

```console
(cd ext/grpc ; git submodule update --init --recursive)
```

Then compile as usual:

```console
mkdir .build
cd .build
cmake ..
make -j $(nproc)
```

### Running the test

Run the server on multiple ports, e.g.:

```console
./grpc_round_robin_freeze_server 12340 12341 12342 12343 12344 12345
```

Then run the client against this server for a few minutes:

```console
./grpc_round_robin_freeze_client ipv4:127.0.0.1:12340,127.0.0.1:12341,127.0.0.1:12342,127.0.0.1:12343,127.0.0.1:12344,127.0.0.1:12345 4 30
```

The client will eventually freeze and stop reporting progress.
