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

from google.storage.v2 import storage_pb2, storage_pb2_grpc
from google.iam.v1 import iam_policy_pb2
from google.protobuf.empty_pb2 import Empty

server = grpc.server(futures.ThreadPoolExecutor(max_workers=1))
db = None


class StorageServicer(storage_pb2_grpc.StorageServicer):

    # === OBJECT === #

    def handle_insert_object_streaming_rpc(self, request_iterator, context):
        """Process an InsertObject streaming RPC, returning the upload object associated with it."""
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
            elif first_message == "write_object_spec":
                bucket = db.get_bucket_without_generation(
                    request.write_object_spec.resource.bucket, context
                ).metadata
                upload = gcs_type.holder.DataHolder.init_resumable_grpc(
                    request, bucket, context
                )
            else:
                print("WARNING unexpected first message %s\n" % first_message)
                continue
            data = request.WhichOneof("data")
            if data == "checksummed_data":
                checksummed_data = request.checksummed_data
            else:
                print("WARNING unexpected data field %s\n" % data)
                continue
            content = checksummed_data.content
            crc32c_hash = (
                checksummed_data.crc32c if checksummed_data.HasField("crc32c") else None
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
        return upload, is_resumable

    def WriteObject(self, request_iterator, context):
        db.insert_test_bucket(context)
        upload, is_resumable = self.handle_insert_object_streaming_rpc(
            request_iterator, context
        )
        if upload is None:
            return utils.error.missing(
                "missing initial upload_id or insert_object_spec", context
            )
        if not upload.complete:
            if not is_resumable:
                utils.error.missing("finish_write in request", context)
            else:
                return storage_pb2.WriteObjectResponse(committed_size=len(upload.media))
        blob, _ = gcs_type.object.Object.init(
            upload.request, upload.metadata, upload.media, upload.bucket, False, context
        )
        upload.blob = blob
        db.insert_object(upload.request, upload.bucket.name, blob, context)
        return storage_pb2.WriteObjectResponse(resource=blob.metadata)

    def ReadObject(self, request, context):
        # TODO(#6892) - break media in 2MiB responses
        blob = db.get_object(request, request.bucket, request.object, False, context)
        yield storage_pb2.ReadObjectResponse(
            checksummed_data={
                "content": blob.media,
                "crc32c": crc32c.crc32(blob.media),
            },
            metadata=blob.metadata,
        )

    def StartResumableWrite(self, request, context):
        bucket = db.get_bucket_without_generation(
            request.write_object_spec.resource.bucket, context
        ).metadata
        upload = gcs_type.holder.DataHolder.init_resumable_grpc(
            request, bucket, context
        )
        db.insert_upload(upload)
        return storage_pb2.StartResumableWriteResponse(upload_id=upload.upload_id)

    def QueryWriteStatus(self, request, context):
        # TODO(#6892) - support querying completed uploads
        upload = db.get_upload(request.upload_id, context)
        return storage_pb2.QueryWriteStatusResponse(
            committed_size=len(upload.media), resource=upload.blob
        )


def run(port, database):
    global db
    db = database
    storage_pb2_grpc.add_StorageServicer_to_server(StorageServicer(), server)
    port = server.add_insecure_port("localhost:" + port)
    db.raii(server)
    server.start()
    return port
