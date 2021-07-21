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

"""Implement a class to simulate GCS object."""

import base64
import datetime
import hashlib
import json
import random
import re
import struct
import sys
import time

import crc32c
import flask
import utils

from google.storage.v2 import storage_pb2
from google.protobuf import field_mask_pb2, json_format


class Object:
    modifiable_fields = [
        "content_encoding",
        "content_disposition",
        "cache_control",
        "acl",
        "content_language",
        "content_type",
        "storage_class",
        "kms_key",
        "temporary_hold",
        "retention_expire_time",
        "metadata",
        "event_based_hold",
        "customer_encryption",
        "custom_time",
    ]

    def __init__(self, metadata, media, bucket):
        self.metadata = metadata
        self.media = media
        self.bucket = bucket

    @classmethod
    def __insert_predefined_acl(cls, metadata, bucket, predefined_acl, context):
        if (
            predefined_acl == ""
            or predefined_acl
            == storage_pb2.PredefinedObjectAcl.PREDEFINED_OBJECT_ACL_UNSPECIFIED
        ):
            return
        if bucket.iam_config.uniform_bucket_level_access.enabled:
            utils.error.invalid(
                "Predefined ACL with uniform bucket level access enabled", context
            )
        acls = utils.acl.compute_predefined_object_acl(
            metadata.bucket, metadata.name, metadata.generation, predefined_acl, context
        )
        del metadata.acl[:]
        metadata.acl.extend(acls)

    @classmethod
    def __postprocess_object_metadata(cls, metadata):
        """The protos for storage/v2 renamed some fields in ways that require some custom coding."""
        # For some fields the storage/v2 name just needs to change slightly.
        metadata = utils.common.rest_adjust(
            metadata,
            {
                "createTime": lambda x: ("timeCreated", x),
                "updateTime": lambda x: ("updated", x),
                "kmsKey": lambda x: ("kmsKeyName", x),
                "retentionExpireTime": lambda x: ("retentionExpirationTime", x),
                "deleteTime": lambda x: ("timeDeleted", x),
                "updateStorageClassTime": lambda x: ("timeStorageClassUpdated", x),
            },
        )
        metadata["kind"] = "storage#object"
        metadata["id"] = "%s/o/%s#%s" % (
            metadata["bucket"],
            metadata["name"],
            metadata["generation"],
        )
        # Checksums need special treatment
        cs = metadata.pop("checksums", None)
        if cs is not None:
            if "crc32c" in cs:
                metadata["crc32c"] = base64.b64encode(
                    struct.pack(">I", cs["crc32c"])
                ).decode("utf-8")
            if "md5Hash" in cs:
                metadata["md5Hash"] = cs["md5Hash"]
        # Finally the ACLs, if present, require additional fields
        if "acl" in metadata:
            for a in metadata["acl"]:
                a["kind"] = "storage#objectAccessControl"
                a["bucket"] = metadata.get("bucket", None)
                a["object"] = metadata.get("name", None)
                a["generation"] = metadata.get("generation", None)
        return metadata

    @classmethod
    def init(cls, request, metadata, media, bucket, is_destination, context):
        instruction = utils.common.extract_instruction(request, context)
        if instruction == "inject-upload-data-error":
            media = utils.common.corrupt_media(media)
        timestamp = datetime.datetime.now(datetime.timezone.utc)
        metadata.bucket = bucket.name
        metadata.generation = int(timestamp.timestamp() * 1000)
        metadata.metageneration = 1
        metadata.size = len(media)
        actual_md5Hash = hashlib.md5(media).digest()
        actual_crc32c = crc32c.crc32c(media)
        if metadata.HasField("checksums"):
            cs = metadata.checksums
            if len(cs.md5_hash) != 0 and actual_md5Hash != cs.md5_hash:
                utils.error.mismatch("md5Hash", cs.md5_hash, actual_md5Hash, context)
            if cs.HasField("crc32c") and actual_crc32c != cs.crc32c:
                utils.error.mismatch("crc32c", cs.crc32c, actual_crc32c, context)
        metadata.checksums.md5_hash = actual_md5Hash
        metadata.checksums.crc32c = actual_crc32c
        metadata.create_time.FromDatetime(timestamp)
        metadata.update_time.FromDatetime(timestamp)
        metadata.owner.entity = utils.acl.get_object_entity("OWNER", context)
        metadata.owner.entity_id = hashlib.md5(
            metadata.owner.entity.encode("utf-8")
        ).hexdigest()
        algorithm, key_b64, key_sha256_b64 = utils.csek.extract(request, False, context)
        if algorithm != "":
            utils.csek.check(algorithm, key_b64, key_sha256_b64, context)
            metadata.customer_encryption.encryption_algorithm = algorithm
            metadata.customer_encryption.key_sha256 = key_sha256_b64
        default_projection = "noAcl"
        is_uniform = bucket.iam_config.uniform_bucket_level_access.enabled
        bucket.iam_config.uniform_bucket_level_access.enabled = False
        if len(metadata.acl) != 0:
            default_projection = "full"
        else:
            predefined_acl = utils.acl.extract_predefined_acl(
                request, is_destination, context
            )
            if (
                predefined_acl
                == storage_pb2.PredefinedObjectAcl.PREDEFINED_OBJECT_ACL_UNSPECIFIED
            ):
                predefined_acl = (
                    storage_pb2.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE
                )
            elif predefined_acl == "":
                predefined_acl = "projectPrivate"
            elif is_uniform:
                utils.error.invalid(
                    "Predefined ACL with uniform bucket level access enabled", context
                )
            cls.__insert_predefined_acl(metadata, bucket, predefined_acl, context)
        bucket.iam_config.uniform_bucket_level_access.enabled = is_uniform
        return (
            cls(metadata, media, bucket),
            utils.common.extract_projection(request, default_projection, context),
        )

    @classmethod
    def init_dict(cls, request, metadata, media, bucket, is_destination):
        metadata = json_format.ParseDict(
            utils.common.preprocess_object_metadata(metadata), storage_pb2.Object()
        )
        return cls.init(request, metadata, media, bucket, is_destination, None)

    @classmethod
    def init_media(cls, request, bucket):
        object_name = request.args.get("name", None)
        media = utils.common.extract_media(request)
        if object_name is None:
            utils.error.missing("name", None)
        metadata = {
            "bucket": bucket.name,
            "name": object_name,
            "metadata": {"x_emulator_upload": "simple"},
        }
        return cls.init_dict(request, metadata, media, bucket, False)

    @classmethod
    def init_multipart(cls, request, bucket):
        metadata, media_headers, media = utils.common.parse_multipart(request)
        metadata["name"] = request.args.get("name", metadata.get("name", None))
        if metadata["name"] is None:
            utils.error.missing("name", None)
        if (
            metadata.get("contentType") is not None
            and media_headers.get("content-type") is not None
            and metadata.get("contentType") != media_headers.get("content-type")
        ):
            utils.error.mismatch(
                "Content-Type",
                media_headers.get("content-type"),
                metadata.get("contentType"),
            )
        metadata["bucket"] = bucket.name
        if "contentType" not in metadata:
            metadata["contentType"] = media_headers.get("content-type")
        metadata["metadata"] = (
            {} if "metadata" not in metadata else metadata["metadata"]
        )
        metadata["metadata"]["x_emulator_upload"] = "multipart"
        if "md5Hash" in metadata:
            metadata["metadata"]["x_emulator_md5"] = metadata["md5Hash"]
            metadata["md5Hash"] = metadata["md5Hash"]
        if "crc32c" in metadata:
            metadata["metadata"]["x_emulator_crc32c"] = metadata["crc32c"]
            metadata["crc32c"] = metadata["crc32c"]
        return cls.init_dict(request, metadata, media, bucket, False)

    @classmethod
    def init_xml(cls, request, bucket, name):
        media = utils.common.extract_media(request)
        metadata = {
            "bucket": bucket.name,
            "name": name,
            "metadata": {"x_emulator_upload": "xml"},
        }
        if "content-type" in request.headers:
            metadata["contentType"] = request.headers["content-type"]
        fake_request = utils.common.FakeRequest.init_xml(request)
        x_goog_hash = fake_request.headers.get("x-goog-hash")
        if x_goog_hash is not None:
            for checksum in x_goog_hash.split(","):
                if checksum.startswith("md5="):
                    md5Hash = checksum[4:]
                    metadata["md5Hash"] = md5Hash
                if checksum.startswith("crc32c="):
                    crc32c_value = checksum[7:]
                    metadata["crc32c"] = crc32c_value
        blob, _ = cls.init_dict(fake_request, metadata, media, bucket, False)
        return blob, fake_request

    # === METADATA === #

    def __update_metadata(self, source, update_mask):
        if update_mask is None:
            update_mask = field_mask_pb2.FieldMask(paths=Object.modifiable_fields)
        update_mask.MergeMessage(source, self.metadata, True, True)
        if self.bucket.versioning.enabled:
            self.metadata.metageneration += 1
        self.metadata.update_time.FromDatetime(datetime.datetime.now())

    def update(self, request, context):
        metadata = None
        if context is not None:
            metadata = request.metadata
        else:
            data = json.loads(request.data)
            metadata = json_format.ParseDict(utils.common.preprocess_object_metadata(data), storage_pb2.Object())
        self.__update_metadata(metadata, None)
        self.__insert_predefined_acl(
            metadata,
            self.bucket,
            utils.acl.extract_predefined_acl(request, False, context),
            context,
        )

    def patch(self, request, context):
        update_mask = field_mask_pb2.FieldMask()
        metadata = None
        if context is not None:
            metadata = request.metadata
            update_mask = request.update_mask
        else:
            data = json.loads(request.data)
            if "metadata" in data:
                if data["metadata"] is None:
                    self.metadata.metadata.clear()
                else:
                    for key, value in data["metadata"].items():
                        if value is None:
                            self.metadata.metadata.pop(key, None)
                        else:
                            self.metadata.metadata[key] = value
            data.pop("metadata", None)
            metadata = json_format.ParseDict(utils.common.preprocess_object_metadata(data), storage_pb2.Object())
            paths = set()
            for key in utils.common.nested_key(data):
                key = utils.common.to_snake_case(key)
                head = key
                for i, c in enumerate(key):
                    if c == "." or c == "[":
                        head = key[0:i]
                        break
                if head in Object.modifiable_fields:
                    if "[" in key:
                        paths.add(head)
                    else:
                        paths.add(key)
            update_mask = field_mask_pb2.FieldMask(paths=list(paths))
        self.__update_metadata(metadata, update_mask)
        self.__insert_predefined_acl(
            metadata,
            self.bucket,
            utils.acl.extract_predefined_acl(request, False, context),
            context,
        )

    # === ACL === #

    def __search_acl(self, entity, must_exist, context):
        entity = utils.acl.get_canonical_entity(entity)
        for i in range(len(self.metadata.acl)):
            if self.metadata.acl[i].entity == entity:
                return i
        if must_exist:
            utils.error.notfound("ACL %s" % entity, context)

    def __upsert_acl(self, entity, role, context):
        # For simplicity, we treat `insert`, `update` and `patch` ACL the same way.
        index = self.__search_acl(entity, False, context)
        acl = utils.acl.create_object_acl(
            self.metadata.bucket,
            self.metadata.name,
            self.metadata.generation,
            entity,
            role,
            context,
        )
        if index is not None:
            self.metadata.acl[index].CopyFrom(acl)
            return self.metadata.acl[index]
        else:
            self.metadata.acl.append(acl)
            return acl

    def get_acl(self, entity, context):
        index = self.__search_acl(entity, True, context)
        return self.metadata.acl[index]

    def insert_acl(self, request, context):
        entity, role = "", ""
        if context is not None:
            entity, role = (
                request.object_access_control.entity,
                request.object_access_control.role,
            )
        else:
            payload = json.loads(request.data)
            entity, role = payload["entity"], payload["role"]
        return self.__upsert_acl(entity, role, context)

    def update_acl(self, request, entity, context):
        role = ""
        if context is not None:
            role = request.object_access_control.role
        else:
            payload = json.loads(request.data)
            role = payload["role"]
        return self.__upsert_acl(entity, role, context)

    def patch_acl(self, request, entity, context):
        role = ""
        if context is not None:
            role = request.object_access_control.role
        else:
            payload = json.loads(request.data)
            role = payload["role"]
        return self.__upsert_acl(entity, role, context)

    def delete_acl(self, entity, context):
        del self.metadata.acl[self.__search_acl(entity, True, context)]

    # === RESPONSE === #

    @classmethod
    def rest(cls, metadata):
        response = cls.__postprocess_object_metadata(
            json_format.MessageToDict(metadata)
        )
        response["kind"] = "storage#object"
        old_metadata = {}
        if "metadata" in response:
            for key, value in response["metadata"].items():
                if "emulator" in key:
                    old_key = key.replace("emulator", "testbench")
                    old_metadata[old_key] = value
            response["metadata"].update(old_metadata)
        return response

    def rest_metadata(self):
        return self.rest(self.metadata)

    def x_goog_hash_header(self):
        if not self.metadata.HasField("checksums"):
            return None
        hashes = []
        cs = self.metadata.checksums
        if cs.HasField("crc32c"):
            hashes.append(
                "crc32c=%s"
                % utils.common.rest_crc32c_from_proto(cs.crc32c)
            )
        if len(cs.md5_hash) != 0:
            hashes.append("md5=%s" % base64.b64encode(cs.md5_hash).decode("utf-8"))
        return ",".join(hashes) if len(hashes) != 0 else None

    def rest_media(self, request):
        range_header = request.headers.get("range")
        response_payload = self.media
        begin = 0
        end = len(response_payload)
        if range_header is not None:
            m = re.match("bytes=([0-9]+)-([0-9]+)", range_header)
            if m:
                begin = int(m.group(1))
                end = int(m.group(2))
                response_payload = response_payload[begin : end + 1]
            m = re.match("bytes=([0-9]+)-$", range_header)
            if m:
                begin = int(m.group(1))
                response_payload = response_payload[begin:]
            m = re.match("bytes=-([0-9]+)$", range_header)
            if m:
                last = int(m.group(1))
                response_payload = response_payload[-last:]

        streamer, length, headers = None, len(response_payload), {}
        content_range = "bytes %d-%d/%d" % (begin, end - 1, length)

        instructions = utils.common.extract_instruction(request, None)
        if instructions == "return-broken-stream":
            headers["Content-Length"] = length

            def streamer():
                chunk_size = 64 * 1024
                for r in range(0, len(response_payload), chunk_size):
                    if r > 1024 * 1024:
                        print("\n\n###### EXIT to simulate crash\n")
                        sys.exit(1)
                    time.sleep(0.1)
                    chunk_end = min(r + chunk_size, len(response_payload))
                    yield response_payload[r:chunk_end]

        elif instructions == "return-corrupted-data":
            media = utils.common.corrupt_media(response_payload)

            def streamer():
                yield media

        elif instructions is not None and instructions.startswith(u"stall-always"):

            def streamer():
                chunk_size = 16 * 1024
                for r in range(begin, end, chunk_size):
                    chunk_end = min(r + chunk_size, end)
                    if r == begin:
                        time.sleep(10)
                    yield response_payload[r:chunk_end]

        elif instructions == "stall-at-256KiB" and begin == 0:

            def streamer():
                chunk_size = 16 * 1024
                for r in range(begin, end, chunk_size):
                    chunk_end = min(r + chunk_size, end)
                    if r == 256 * 1024:
                        time.sleep(10)
                    yield response_payload[r:chunk_end]

        elif instructions is not None and instructions.startswith(
            u"return-503-after-256K"
        ):
            if begin == 0:

                def streamer():
                    chunk_size = 4 * 1024
                    for r in range(0, len(response_payload), chunk_size):
                        if r >= 256 * 1024:
                            print("\n\n###### EXIT to simulate crash\n")
                            sys.exit(1)
                        time.sleep(0.01)
                        chunk_end = min(r + chunk_size, len(response_payload))
                        yield response_payload[r:chunk_end]

            elif instructions.endswith(u"/retry-1"):
                return flask.Response("Service Unavailable / retry-1", status=503)
            elif instructions.endswith(u"/retry-2"):
                return flask.Response("Service Unavailable / retry-2", status=503)
            else:
                return flask.Response(response_payload, status=200, headers=headers)
        else:

            def streamer():
                yield response_payload

        headers["Content-Range"] = content_range
        headers["x-goog-hash"] = self.x_goog_hash_header()
        headers["x-goog-generation"] = self.metadata.generation
        return flask.Response(streamer(), status=200, headers=headers)
