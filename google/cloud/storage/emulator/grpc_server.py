# Copyright 2020 Google LLC
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

from concurrent import futures

import crc32c
import gcs as gcs_type
import grpc
import utils

from google.cloud.storage_v1.proto import storage_pb2, storage_pb2_grpc
from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
from google.protobuf.empty_pb2 import Empty

server = grpc.server(futures.ThreadPoolExecutor(max_workers=1))
db = None


class StorageServicer(storage_pb2_grpc.StorageServicer):

    # === BUCKET ===#

    def ListBuckets(self, request, context):
        db.insert_test_bucket(context)
        result = resources_pb2.ListBucketsResponse(next_page_token="", items=[])
        for bucket in db.list_bucket(request, request.project, context):
            result.items.append(bucket.metadata)
        return result

    def InsertBucket(self, request, context):
        db.insert_test_bucket(context)
        bucket, projection = gcs_type.bucket.Bucket.init(request, context)
        db.insert_bucket(request, bucket, context)
        return bucket.metadata

    def GetBucket(self, request, context):
        bucket_name = request.bucket
        bucket = db.get_bucket(request, bucket_name, context)
        return bucket.metadata

    def DeleteBucket(self, request, context):
        bucket_name = request.bucket
        db.delete_bucket(request, bucket_name, context)
        return Empty()

    # === OBJECT === #

    def InsertObject(self, request_iterator, context):
        db.insert_test_bucket(context)
        upload, is_resumable = None, False
        for request in request_iterator:
            first_message = request.WhichOneof("first_message")
            if first_message == "upload_id":
                upload = db.get_upload(request.upload_id, context)
                if upload.complete:
                    utils.error.invalid(
                        "Uploading to a completed upload %s" % upload.upload_id, context
                    )
                is_resumable = True
            elif first_message == "insert_object_spec":
                bucket = db.get_bucket_without_generation(
                    request.insert_object_spec.resource.bucket, context
                ).metadata
                upload = gcs_type.holder.DataHolder.init_resumable_grpc(
                    request, bucket, context
                )
            data = request.WhichOneof("data")
            checksummed_data = None
            if data == "checksummed_data":
                checksummed_data = request.checksummed_data
            elif data == "reference":
                checksummed_data = self.GetObjectMedia(
                    data.reference, context
                ).checksummed_data
            else:
                continue
            content = checksummed_data.content
            crc32c_hash = (
                checksummed_data.crc32c.value
                if checksummed_data.HasField("crc32c")
                else None
            )
            if crc32c_hash is not None:
                actual_crc32c = crc32c.crc32(content)
                if actual_crc32c != crc32c_hash:
                    utils.error.mismatch(
                        "crc32c in checksummed data",
                        crc32c_hash,
                        actual_crc32c,
                        context,
                    )
            upload.media += checksummed_data.content
            if request.finish_write:
                upload.complete = True
                break
        if not upload.complete:
            if not is_resumable:
                utils.error.missing("finish_write in request", context)
            else:
                return
        blob, _ = gcs_type.object.Object.init(
            upload.request, upload.metadata, upload.media, upload.bucket, False, context
        )
        db.insert_object(upload.request, upload.bucket.name, blob, context)
        return blob.metadata

    def GetObjectMedia(self, request, context):
        blob = db.get_object(request, request.bucket, request.object, False, context)
        yield storage_pb2.GetObjectMediaResponse(
            checksummed_data={
                "content": blob.media,
                "crc32c": {"value": crc32c.crc32(blob.media)},
            },
            metadata=blob.metadata,
        )

    def DeleteObject(self, request, context):
        db.delete_object(request, request.bucket, request.object, context)
        return Empty()

    def StartResumableWrite(self, request, context):
        bucket = db.get_bucket_without_generation(
            request.insert_object_spec.resource.bucket, context
        ).metadata
        upload = gcs_type.holder.DataHolder.init_resumable_grpc(
            request, bucket, context
        )
        upload.metadata.metadata["x_emulator_upload"] = "resumable"
        db.insert_upload(upload)
        return storage_pb2.StartResumableWriteResponse(upload_id=upload.upload_id)

    def QueryWriteStatus(self, request, context):
        upload = db.get_upload(request.upload_id, context)
        return storage_pb2.QueryWriteStatusResponse(
            committed_size=len(upload.media), complete=upload.complete
        )


def run(port, database):
    global db
    db = database
    storage_pb2_grpc.add_StorageServicer_to_server(StorageServicer(), server)
    port = server.add_insecure_port("localhost:" + port)
    db.raii(server)
    server.start()
    return port
