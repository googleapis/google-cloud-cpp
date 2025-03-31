// Copyright 2025 Google LLC


#include <pybind11/pybind11.h>
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include <iostream>
#include <sstream>
#include <string>

namespace py = pybind11;
namespace gcs = google::cloud::storage;

std::string quickstart_grpc_pybind(const std::string& bucket_name) {
  auto client = gcs::MakeGrpcClient();

  auto writer = client.WriteObject(bucket_name, "quickstart-grpc.txt");
  writer << "Hello World!";
  writer.Close();

  if (!writer.metadata()) {
    std::ostringstream error_message;
    error_message << "Error creating object: " << writer.metadata().status();
    return error_message.str();
  }

  std::ostringstream output;
  output << "Successfully created object: " << *writer.metadata() << "\n";

  auto reader = client.ReadObject(bucket_name, "quickstart-grpc.txt");
  if (!reader) {
    output << "Error reading object: " << reader.status() << "\n";
    return output.str();
  }

  std::string contents{std::istreambuf_iterator<char>{reader}, {}};
  output << contents << "\n";

  return output.str();
}

PYBIND11_MODULE(google_cloud_storage_grpc_binding, m) {
  m.def("quickstart_grpc", &quickstart_grpc_pybind,
        "Performs a Google Cloud Storage gRPC quickstart operation.",
        py::arg("bucket_name"));
}
