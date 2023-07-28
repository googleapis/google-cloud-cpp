# CBT Test Proxy

This project contains conformance tests for the Cloud Bigtable C++ client
library. It is a way for Google's developers to verify that the library exhibits
correct behavior. It is unlikely to be of value to external customers.

## How to run the conformance tests

### Setup

If you have not already done so, [install golang](https://go.dev/doc/install),
then clone the libraries:

```sh
git clone https://github.com/googleapis/google-cloud-cpp.git
git clone https://github.com/googleapis/cloud-bigtable-clients-test.git
```

### Run test proxy server

Open a terminal and navigate to the test proxy directory. Then run the server.
In this example, we use port 9999.

```sh
cd google-cloud-cpp/google/cloud/bigtable/test_proxy
env GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes bazel run :cbt_test_proxy_main -- 9999
```

### Run the test cases

Open another terminal and navigate to the test runner directory. Then run the
tests:

```sh
cd cloud-bigtable-clients-test/tests
go test -v -proxy_addr=:9999
```

## Notes

The CMake files are used to run sanitizers/linters on the code. For building and
testing, Bazel is recommended (as shown above).
