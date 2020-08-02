#!/usr/bin/env python3
# Copyright 2020 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""A test bench for the GCS+gRPC plugin C++ Client Library."""

import grpc
import logging
import testbench_utils
from concurrent import futures
from google.protobuf.empty_pb2 import Empty
from google.cloud.storage_v1.proto import storage_pb2, storage_pb2_grpc, storage_resources_pb2, storage_resources_pb2_grpc
from pprint import pprint


class StorageServicer(storage_pb2_grpc.StorageServicer):
    """Provides methods that implement functionality of GCS+gRPC server."""

    def InsertBucket(self, request, context):
        name = request.bucket.name
        if testbench_utils.has_bucket(name):
            context.set_details("Bucket %s already exists" % name)
            context.set_code(grpc.StatusCode.ALREADY_EXISTS)
            return storage_resources_pb2.Bucket()
        bucket = storage_resources_pb2.Bucket(name=name, id=name)
        return testbench_utils.insert_bucket(name, bucket)

    def ListBuckets(self, request, context):
        result = storage_resources_pb2.ListBucketsResponse(next_page_token="")
        for name, bucket in testbench_utils.all_buckets():
            result.items.append(bucket)
        return result

    def GetBucket(self, request, context):
        name = request.bucket
        if not testbench_utils.has_bucket(name):
            context.set_details("Bucket %s does not exist" % name)
            context.set_code(grpc.StatusCode.NOT_FOUND)
            return storage_resources_pb2.Bucket()
        return testbench_utils.get_bucket(name)

    def DeleteBucket(self, request, context):
        name = request.bucket
        if not testbench_utils.has_bucket(name):
            context.set_details("Bucket %s does not exist" % name)
            context.set_code(grpc.StatusCode.NOT_FOUND)
        else:
            testbench_utils.delete_bucket(name)
        return Empty()

    def InsertObject(self, request_iterator, context):
        value = None
        for request in request_iterator:
            # non-resumable uploads
            if request.insert_object_spec is not None:
                spec = request.insert_object_spec
                value = spec.resource
                content = request.checksummed_data.content
                testbench_utils.insert_object(value, content)
                # TODO(vnvo2409): Check for md5_hash and crc32c.
            #TODO(vnvo2409): Resumable uploads
        return value

    def GetObjectMedia(self, request, context):
        bucket_name = request.bucket
        object_name = request.object
        value = testbench_utils.get_object(bucket_name, object_name)
        yield storage_pb2.GetObjectMediaResponse(checksummed_data = {"content": value['content']})

    def DeleteObject(self, request, context):
        bucket_name = request.bucket
        object_name = request.object
        if not testbench_utils.has_object(bucket_name, object_name):
            context.set_details("Object %s does not exist" % object_name)
            context.set_code(grpc.StatusCode.NOT_FOUND)
        else:
            testbench_utils.delete_object(bucket_name, object_name)
        return Empty()


def main():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    storage_pb2_grpc.add_StorageServicer_to_server(StorageServicer(), server)
    server.add_insecure_port('localhost:8585')
    server.start()
    server.wait_for_termination()

if __name__ == '__main__':
    logging.basicConfig()
    main()
