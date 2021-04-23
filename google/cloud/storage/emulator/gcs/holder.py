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

"""Implement a holder for resumable upload's data and rewrite's data"""

import hashlib
import json
import types

import flask
import utils

from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
from google.protobuf import json_format


class DataHolder(types.SimpleNamespace):
    rest_only_fields = ["customTime"]

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    # === UPLOAD === #

    @classmethod
    def __extract_rest_only(cls, data):
        rest_only = {}
        for field in DataHolder.rest_only_fields:
            if field in data:
                rest_only[field] = data.pop(field)
        return rest_only

    @classmethod
    def init_upload(
        cls, request, metadata, bucket, location, upload_id, rest_only=None
    ):
        return cls(
            request=request,
            metadata=metadata,
            bucket=bucket,
            location=location,
            upload_id=upload_id,
            media=b"",
            complete=False,
            transfer=set(),
            rest_only=rest_only,
        )

    @classmethod
    def init_resumable_rest(cls, request, bucket):
        name = request.args.get("name", "")
        rest_only = {}
        if len(request.data) > 0 and request.data.decode("utf-8") != "{}":
            if name != "":
                utils.error.invalid("name argument in non-empty payload", None)
            data = json.loads(request.data)
            rest_only = cls.__extract_rest_only(data)
            metadata = json_format.ParseDict(data, resources_pb2.Object())
        else:
            metadata = resources_pb2.Object()
            metadata.name = name
        if metadata.content_type == "":
            metadata.content_type = request.headers.get(
                "x-upload-content-type", "application/octet-stream"
            )
        upload_id = hashlib.sha256(
            ("%s/o/%s" % (bucket.name, metadata.name)).encode("utf-8")
        ).hexdigest()
        location = (
            request.host_url
            + "upload/storage/v1/b/%s/o?uploadType=resumable&upload_id=%s"
            % (bucket.name, upload_id)
        )
        headers = {
            key.lower(): value
            for key, value in request.headers.items()
            if key.lower().startswith("x-")
        }
        request = utils.common.FakeRequest(
            args=request.args.to_dict(), headers=headers, data=b""
        )
        return cls.init_upload(
            request, metadata, bucket, location, upload_id, rest_only
        )

    @classmethod
    def init_resumable_grpc(cls, request, bucket, context):
        metadata = request.insert_object_spec.resource
        upload_id = hashlib.sha256(
            ("%s/o/%s" % (bucket.name, metadata.name)).encode("utf-8")
        ).hexdigest()
        fake_request = utils.common.FakeRequest.init_protobuf(request, context)
        fake_request.update_protobuf(request.insert_object_spec, context)
        return cls.init_upload(fake_request, metadata, bucket, "", upload_id)

    def resumable_status_rest(self):
        response = flask.make_response()
        if len(self.media) > 1 and not self.complete:
            response.headers["Range"] = "bytes=0-%d" % (len(self.media) - 1)
        response.status_code = 308
        return response

    # === REWRITE === #

    @classmethod
    def init_rewrite_rest(
        cls, request, src_bucket_name, src_object_name, dst_bucket_name, dst_object_name
    ):
        fake_request = utils.common.FakeRequest(
            args=request.args.to_dict(),
            headers={
                key.lower(): value
                for key, value in request.headers.items()
                if key.lower().startswith("x-")
            },
            data=request.data,
        )
        max_bytes_rewritten_per_call = min(
            int(fake_request.args.get("maxBytesRewrittenPerCall", 1024 * 1024)),
            1024 * 1024,
        )
        token = hashlib.sha256(
            (
                "%s/o/%s/rewriteTo/b/%s/o/%s"
                % (src_bucket_name, src_object_name, dst_bucket_name, dst_object_name)
            ).encode("utf-8")
        ).hexdigest()
        return cls(
            request=fake_request,
            src_bucket_name=src_bucket_name,
            src_object_name=src_object_name,
            dst_bucket_name=dst_bucket_name,
            dst_object_name=dst_object_name,
            token=token,
            media=b"",
            max_bytes_rewritten_per_call=max_bytes_rewritten_per_call,
        )
