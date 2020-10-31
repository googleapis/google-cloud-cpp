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

"""Implement a class as a placeholder for data."""

import types
from google.protobuf import json_format
from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
import simdjson
import utils
import hashlib
import flask


class DataHolder(types.SimpleNamespace):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    @classmethod
    def init_upload(cls, request, metadata, bucket, location, upload_id):
        return cls(
            request=request,
            metadata=metadata,
            bucket=bucket,
            location=location,
            upload_id=upload_id,
            media=b"",
            complete=False,
            transfer=set(),
        )

    @classmethod
    def init_resumable_rest(cls, request, bucket):
        name = request.args.get("name", "")
        if len(request.data) > 0:
            if name != "":
                utils.error.invalid("name argument in non-empty payload", None)
            data = simdjson.loads(request.data)
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
        return cls.init_upload(request, metadata, bucket, location, upload_id)

    @classmethod
    def init_resumable_grpc(cls, request, bucket):
        metadata = request.insert_object_spec.resource
        upload_id = hashlib.sha256(
            ("%s/o/%s" % (bucket.name, metadata.name)).encode("utf-8")
        ).hexdigest()
        return cls.init_upload(request, metadata, bucket, "", upload_id)

    def resumable_status_rest(self):
        response = flask.make_response()
        if len(self.media) > 1 and not self.complete:
            response.headers["Range"] = "bytes=0-%d" % (len(self.media) - 1)
        response.status_code = 308
        return response
