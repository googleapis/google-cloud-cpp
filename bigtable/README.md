To build and run with the latest protobuf and grpc, follow the steps below.

1.  **Compile and Test:**
    ```sh
    cd google-cloud-cpp/
    git submodule init
    git submodule update --init --recursive
    mkdir build
    cd build
    cmake ..
    make all
    make test
    ```
