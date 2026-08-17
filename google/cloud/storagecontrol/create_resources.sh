#!/bin/bash
curl -s -o /dev/null "http://localhost:9000/start_grpc?port=9090"
curl -s -o /dev/null -X POST --data-binary '{"name": "cloud-cpp-testing-folder-bucket"}' \
  -H "Content-Type: application/json" \
  "http://localhost:9000/storage/v1/b?project=cloud-cpp-testing-resources"
echo "Resources created and gRPC started on port 9090"
