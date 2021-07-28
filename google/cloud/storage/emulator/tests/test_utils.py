#!/usr/bin/env python3
#
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

"""Unit test for utils"""

import http
import os
import unittest
import urllib
import gzip
import socket
import utils
import json

from werkzeug.test import create_environ
from werkzeug.wrappers import Request
from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2
from google.cloud.storage_v1.proto.storage_resources_pb2 import CommonEnums


class TestACL(unittest.TestCase):
    def test_extract_predefined_default_object_acl(self):
        request = storage_pb2.InsertBucketRequest()
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, ""
        )
        self.assertEqual(
            predefined_default_object_acl,
            CommonEnums.PredefinedObjectAcl.PREDEFINED_OBJECT_ACL_UNSPECIFIED,
        )

        request.predefined_default_object_acl = (
            CommonEnums.PredefinedObjectAcl.OBJECT_ACL_AUTHENTICATED_READ
        )
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, ""
        )
        self.assertEqual(
            predefined_default_object_acl,
            CommonEnums.PredefinedObjectAcl.OBJECT_ACL_AUTHENTICATED_READ,
        )

        request = utils.common.FakeRequest(args={})
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, None
        )
        self.assertEqual(predefined_default_object_acl, "")

        request.args["predefinedDefaultObjectAcl"] = "authenticatedRead"
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, None
        )
        self.assertEqual(predefined_default_object_acl, "authenticatedRead")

    def test_extract_predefined_acl(self):
        request = storage_pb2.InsertBucketRequest()
        predefined_acl = utils.acl.extract_predefined_acl(request, False, "")
        self.assertEqual(
            predefined_acl,
            CommonEnums.PredefinedBucketAcl.PREDEFINED_BUCKET_ACL_UNSPECIFIED,
        )

        request.predefined_acl = (
            CommonEnums.PredefinedBucketAcl.BUCKET_ACL_AUTHENTICATED_READ
        )
        predefined_acl = utils.acl.extract_predefined_acl(request, False, "")
        self.assertEqual(
            predefined_acl,
            CommonEnums.PredefinedBucketAcl.BUCKET_ACL_AUTHENTICATED_READ,
        )

        request = storage_pb2.CopyObjectRequest(
            destination_predefined_acl=CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PRIVATE
        )
        predefined_acl = utils.acl.extract_predefined_acl(request, True, "")
        self.assertEqual(
            predefined_acl, CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PRIVATE
        )

        request = utils.common.FakeRequest(args={})
        predefined_acl = utils.acl.extract_predefined_acl(request, False, None)
        self.assertEqual(predefined_acl, "")

        request.args["predefinedAcl"] = "authenticatedRead"
        predefined_acl = utils.acl.extract_predefined_acl(request, False, None)
        self.assertEqual(predefined_acl, "authenticatedRead")

        request.args["destinationPredefinedAcl"] = "bucketOwnerFullControl"
        predefined_acl = utils.acl.extract_predefined_acl(request, True, None)
        self.assertEqual(predefined_acl, "bucketOwnerFullControl")

    def test_compute_predefined_bucket_acl(self):
        acls = utils.acl.compute_predefined_bucket_acl(
            "bucket", "authenticatedRead", None
        )
        entities = [acl.entity for acl in acls]
        self.assertListEqual(
            entities,
            [utils.acl.get_project_entity("owners", None), "allAuthenticatedUsers"],
        )

    def test_compute_predefined_default_object_acl(self):
        acls = utils.acl.compute_predefined_default_object_acl(
            "bucket", "authenticatedRead", None
        )
        entities = [acl.entity for acl in acls]
        self.assertEqual(
            entities,
            [utils.acl.get_object_entity("OWNER", None), "allAuthenticatedUsers"],
        )

        object_names = [acl.object for acl in acls]
        self.assertEqual(object_names, 2 * [""])

    def test_compute_predefined_object_acl(self):
        acls = utils.acl.compute_predefined_object_acl(
            "bucket", "object", 123456789, "authenticatedRead", None
        )
        entities = [acl.entity for acl in acls]
        self.assertEqual(
            entities,
            [utils.acl.get_object_entity("OWNER", None), "allAuthenticatedUsers"],
        )

        object_names = [acl.object for acl in acls]
        self.assertEqual(object_names, 2 * ["object"])

        generations = [acl.generation for acl in acls]
        self.assertEqual(generations, 2 * [123456789])


class TestCommonUtils(unittest.TestCase):
    def test_snake_case(self):
        self.assertEqual(
            utils.common.to_snake_case("authenticatedRead"), "authenticated_read"
        )
        self.assertEqual(
            utils.common.to_snake_case("allAuthenticatedUsers"),
            "all_authenticated_users",
        )

    def test_parse_fields(self):
        fields = "kind, items ( acl( entity, role), name, id)"
        self.assertCountEqual(
            utils.common.parse_fields(fields),
            ["kind", "items.acl.entity", "items.acl.role", "items.name", "items.id"],
        )

        fields = "kind, items(name, labels(number), acl(role))"
        self.assertCountEqual(
            utils.common.parse_fields(fields),
            ["kind", "items.name", "items.labels.number", "items.acl.role"],
        )

    def test_remove_index(self):
        key = "items[1].name[0].id[0].acl"
        self.assertEqual(utils.common.remove_index(key), "items.name.id.acl")

    def test_nested_key(self):
        doc = {
            "name": "bucket",
            "acl": [{"id": 1}, {"id": 2}],
            "labels": {"first": 1, "second": [1, 2]},
        }
        self.assertCountEqual(
            utils.common.nested_key(doc),
            [
                "name",
                "acl[0].id",
                "acl[1].id",
                "labels.first",
                "labels.second[0]",
                "labels.second[1]",
            ],
        )

    def test_extract_projection(self):
        request = storage_pb2.CopyObjectRequest()
        projection = utils.common.extract_projection(
            request, CommonEnums.Projection.NO_ACL, ""
        )
        self.assertEqual(projection, CommonEnums.Projection.NO_ACL)
        request.projection = CommonEnums.Projection.FULL
        projection = utils.common.extract_projection(
            request, CommonEnums.Projection.NO_ACL, ""
        )
        self.assertEqual(projection, CommonEnums.Projection.FULL)

        request = utils.common.FakeRequest(args={})
        projection = utils.common.extract_projection(
            request, CommonEnums.Projection.NO_ACL, None
        )
        self.assertEqual(projection, "noAcl")
        request.args["projection"] = "full"
        projection = utils.common.extract_projection(
            request, CommonEnums.Projection.NO_ACL, None
        )
        self.assertEqual(projection, "full")

    def test_filter_response_rest(self):
        response = {
            "kind": "storage#buckets",
            "items": [
                {
                    "name": "bucket1",
                    "labels": {"number": "1", "order": "1"},
                    "acl": [{"entity": "entity", "role": "OWNER"}],
                },
                {
                    "name": "bucket2",
                    "labels": {"number": "2", "order": "2"},
                    "acl": [{"entity": "entity", "role": "OWNER"}],
                },
                {
                    "name": "bucket3",
                    "labels": {"number": "3", "order": "3"},
                    "acl": [{"entity": "entity", "role": "OWNER"}],
                },
            ],
        }
        response_full = utils.common.filter_response_rest(
            response, "full", "kind, items(name, labels(number), acl(role))"
        )
        self.assertDictEqual(
            response_full,
            {
                "kind": "storage#buckets",
                "items": [
                    {
                        "name": "bucket1",
                        "labels": {"number": "1"},
                        "acl": [{"role": "OWNER"}],
                    },
                    {
                        "name": "bucket2",
                        "labels": {"number": "2"},
                        "acl": [{"role": "OWNER"}],
                    },
                    {
                        "name": "bucket3",
                        "labels": {"number": "3"},
                        "acl": [{"role": "OWNER"}],
                    },
                ],
            },
        )

        response = {
            "kind": "storage#buckets",
            "items": [
                {
                    "name": "bucket1",
                    "labels": {"number": "1", "order": "1"},
                    "acl": [{"entity": "entity", "role": "OWNER"}],
                },
                {
                    "name": "bucket2",
                    "labels": {"number": "2", "order": "2"},
                    "acl": [{"entity": "entity", "role": "OWNER"}],
                },
                {
                    "name": "bucket3",
                    "labels": {"number": "3", "order": "3"},
                    "acl": [{"entity": "entity", "role": "OWNER"}],
                },
            ],
        }
        response_noacl = utils.common.filter_response_rest(
            response, "noAcl", "items(name, labels)"
        )
        self.assertDictEqual(
            response_noacl,
            {
                "items": [
                    {"name": "bucket1", "labels": {"number": "1", "order": "1"}},
                    {"name": "bucket2", "labels": {"number": "2", "order": "2"}},
                    {"name": "bucket3", "labels": {"number": "3", "order": "3"}},
                ]
            },
        )

    def test_parse_multipart(self):
        request = utils.common.FakeRequest(
            headers={"content-type": "multipart/related; boundary=foo_bar_baz"},
            data=b'--foo_bar_baz\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{"name": "myObject", "metadata": {"test": "test"}}\r\n--foo_bar_baz\r\nContent-Type: image/jpeg\r\n\r\n123456789\r\n--foo_bar_baz--\r\n',
            environ={},
        )
        metadata, media_header, media = utils.common.parse_multipart(request)
        self.assertDictEqual(
            metadata, {"name": "myObject", "metadata": {"test": "test"}}
        )
        self.assertDictEqual(media_header, {"content-type": "image/jpeg"})
        self.assertEqual(media, b"123456789")

        # In some cases, data media contains "\r\n" which could confuse `parse_multipart`
        request = utils.common.FakeRequest(
            headers={"content-type": "multipart/related; boundary=1VvZTD07ltUtqMHg"},
            data=b'--1VvZTD07ltUtqMHg\r\ncontent-type: application/json; charset=UTF-8\r\n\r\n{"crc32c":"4GEvYA=="}\r\n--1VvZTD07ltUtqMHg\r\ncontent-type: application/octet-stream\r\n\r\n\xa7#\x95\xec\xd5c\xe9\x90\xa8\xe2\xa89\xadF\xcc\x97\x12\xad\xf6\x9e\r\n\xf1Mhj\xf4W\x9f\x92T\xe3,\tm.\x1e\x04\xd0\r\n--1VvZTD07ltUtqMHg--\r\n',
            environ={},
        )
        metadata, media_header, media = utils.common.parse_multipart(request)
        self.assertDictEqual(metadata, {"crc32c": "4GEvYA=="})
        self.assertDictEqual(media_header, {"content-type": "application/octet-stream"})
        self.assertEqual(
            media,
            b"\xa7#\x95\xec\xd5c\xe9\x90\xa8\xe2\xa89\xadF\xcc\x97\x12\xad\xf6\x9e\r\n\xf1Mhj\xf4W\x9f\x92T\xe3,\tm.\x1e\x04\xd0",
        )

        # Test line ending without "\r\n"
        request = utils.common.FakeRequest(
            headers={"content-type": "multipart/related; boundary=1VvZTD07ltUtqMHg"},
            data=b'--1VvZTD07ltUtqMHg\r\ncontent-type: application/json; charset=UTF-8\r\n\r\n{"crc32c":"4GEvYA=="}\r\n--1VvZTD07ltUtqMHg\r\ncontent-type: application/octet-stream\r\n\r\n\xa7#\x95\xec\xd5c\xe9\x90\xa8\xe2\xa89\xadF\xcc\x97\x12\xad\xf6\x9e\r\n\xf1Mhj\xf4W\x9f\x92T\xe3,\tm.\x1e\x04\xd0\r\n--1VvZTD07ltUtqMHg--',
            environ={},
        )
        metadata, media_header, media = utils.common.parse_multipart(request)
        self.assertDictEqual(metadata, {"crc32c": "4GEvYA=="})
        self.assertDictEqual(media_header, {"content-type": "application/octet-stream"})
        self.assertEqual(
            media,
            b"\xa7#\x95\xec\xd5c\xe9\x90\xa8\xe2\xa89\xadF\xcc\x97\x12\xad\xf6\x9e\r\n\xf1Mhj\xf4W\x9f\x92T\xe3,\tm.\x1e\x04\xd0",
        )

        # Test incorrect multipart body
        request = utils.common.FakeRequest(
            headers={"content-type": "multipart/related; boundary=1VvZTD07ltUtqMHg"},
            data=b'--1VvZTD07ltUtqMHg\r\ncontent-type: application/json; charset=UTF-8\r\n{"crc32c":"4GEvYA=="}\r\n--1VvZTD07ltUtqMHg\r\ncontent-type: application/octet-stream\r\n\xa7#\x95\xec\xd5c\xe9\x90\xa8\xe2\xa89\xadF\xcc\x97\x12\xad\xf6\x9e\r\n\xf1Mhj\xf4W\x9f\x92T\xe3,\tm.\x1e\x04\xd0\r\n',
            environ={},
        )
        with self.assertRaises(utils.error.RestException):
            utils.common.parse_multipart(request)

    def test_enforce_patch_override_failure(self):
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=0,
            data="",
            content_type="application/octet-stream",
            method="POST",
            headers={"X-Http-Method-Override": "other"},
        )
        with self.assertRaises(utils.error.RestException):
            utils.common.enforce_patch_override(Request(environ))

    def test_enforce_patch_override_success(self):
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=0,
            data="",
            content_type="application/octet-stream",
            method="POST",
            headers={"X-Http-Method-Override": "PATCH"},
        )
        utils.common.enforce_patch_override(Request(environ))


class TestGeneration(unittest.TestCase):
    def test_extract_precondition(self):
        request = storage_pb2.CopyObjectRequest(
            if_generation_not_match={"value": 1},
            if_metageneration_match={"value": 2},
            if_metageneration_not_match={"value": 3},
            if_source_generation_match={"value": 4},
            if_source_generation_not_match={"value": 5},
            if_source_metageneration_match={"value": 6},
            if_source_metageneration_not_match={"value": 7},
        )
        match, not_match = utils.generation.extract_precondition(
            request, False, False, ""
        )
        self.assertIsNone(match)
        self.assertEqual(not_match, 1)
        match, not_match = utils.generation.extract_precondition(
            request, True, False, ""
        )
        self.assertEqual(match, 2)
        self.assertEqual(not_match, 3)
        match, not_match = utils.generation.extract_precondition(
            request, False, True, ""
        )
        self.assertEqual(match, 4)
        self.assertEqual(not_match, 5)
        match, not_match = utils.generation.extract_precondition(
            request, True, True, ""
        )
        self.assertEqual(match, 6)
        self.assertEqual(not_match, 7)

        request = utils.common.FakeRequest(
            args={
                "ifGenerationNotMatch": 1,
                "ifMetagenerationMatch": 2,
                "ifMetagenerationNotMatch": 3,
                "ifSourceGenerationMatch": 4,
                "ifSourceGenerationNotMatch": 5,
                "ifSourceMetagenerationMatch": 6,
                "ifSourceMetagenerationNotMatch": 7,
            }
        )
        match, not_match = utils.generation.extract_precondition(
            request, False, False, None
        )
        self.assertIsNone(match)
        self.assertEqual(not_match, 1)
        match, not_match = utils.generation.extract_precondition(
            request, True, False, None
        )
        self.assertEqual(match, 2)
        self.assertEqual(not_match, 3)
        match, not_match = utils.generation.extract_precondition(
            request, False, True, None
        )
        self.assertEqual(match, 4)
        self.assertEqual(not_match, 5)
        match, not_match = utils.generation.extract_precondition(
            request, True, True, None
        )
        self.assertEqual(match, 6)
        self.assertEqual(not_match, 7)

    def test_extract_generation(self):
        request = storage_pb2.GetObjectRequest()
        generation = utils.generation.extract_generation(request, False, "")
        self.assertEqual(generation, 0)

        request.generation = 1
        generation = utils.generation.extract_generation(request, False, "")
        self.assertEqual(generation, 1)

        request = storage_pb2.CopyObjectRequest(source_generation=2)
        generation = utils.generation.extract_generation(request, True, "")
        self.assertEqual(generation, 2)

        request = utils.common.FakeRequest(args={})
        generation = utils.generation.extract_generation(request, False, None)
        self.assertEqual(generation, 0)

        request.args["generation"] = 1
        request.args["sourceGeneration"] = 2
        generation = utils.generation.extract_generation(request, False, None)
        self.assertEqual(generation, 1)
        generation = utils.generation.extract_generation(request, True, None)
        self.assertEqual(generation, 2)


class TestError(unittest.TestCase):
    def test_error_propagation(self):
        emulator = os.getenv("CLOUD_STORAGE_EMULATOR_ENDPOINT")
        if emulator is None:
            self.skipTest("CLOUD_STORAGE_EMULATOR_ENDPOINT is not set")
        conn = http.client.HTTPConnection(emulator[len("http://") :], timeout=10)

        conn.request("GET", "/raise_error")
        response = conn.getresponse()
        body = response.read().decode("utf-8")
        self.assertIn("Traceback (most recent call last):", body)
        self.assertIn("Exception", body)

        params = urllib.parse.urlencode({"etype": "yes", "msg": "custom message"})
        conn.request("GET", "/raise_error?" + params)
        response = conn.getresponse()
        body = response.read().decode("utf-8")
        self.assertIn("Traceback (most recent call last):", body)
        self.assertIn("TypeError", body)
        self.assertIn("custom message", body)
        conn.close()


class TestRetryTest(unittest.TestCase):
    def setUp(self):
        self.emulator = os.getenv("CLOUD_STORAGE_EMULATOR_ENDPOINT")
        if self.emulator is None:
            self.skipTest("CLOUD_STORAGE_EMULATOR_ENDPOINT is not set")
        self.conn = http.client.HTTPConnection(
            self.emulator[len("http://") :], timeout=10
        )

    def tearDown(self):
        self.conn.close()

    def test_retry_test_resource_crud(self):
        self.conn.request("GET", "/retry_tests")
        response = self.conn.getresponse()
        self.assertEqual(response.code, 200)
        self.conn.close()

        initial_retry_test = {"instructions": {"storage.buckets.list": ["return-503"]}}
        request_body = json.dumps(initial_retry_test)
        self.conn.request("POST", "/retry_test", body=request_body)
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        retry_test = json.loads(body)
        retry_test_id = retry_test.get("id", None)
        self.assertIsNotNone(retry_test_id)
        self.conn.close()

        self.conn.request("GET", "/retry_test/%s" % (retry_test_id))
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        retry_test = json.loads(body)
        self.assertEqual(retry_test["instructions"], initial_retry_test["instructions"])
        self.conn.close()

        self.conn.request("DELETE", "/retry_test/%s" % (retry_test_id))
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        self.assertIn("Deleted %s" % (retry_test_id), body)
        self.conn.close()

    def test_retry_test_reset_connection(self):
        initial_retry_test = {
            "instructions": {"storage.buckets.list": ["return-reset-connection"]}
        }
        request_body = json.dumps(initial_retry_test)
        self.conn.request("POST", "/retry_test", body=request_body)
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        retry_test = json.loads(body)
        retry_test_id = retry_test.get("id", None)
        self.assertIsNotNone(retry_test_id)
        self.conn.close()

        self.conn.request(
            "GET",
            "/storage/v1/b?project=test",
            headers={"x-retry-test-id": retry_test_id},
        )
        with self.assertRaises(ConnectionResetError):
            response = self.conn.getresponse()

        self.conn.request("DELETE", "/retry_test/%s" % (retry_test_id))
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        self.assertIn("Deleted %s" % (retry_test_id), body)
        self.conn.close()

    def test_retry_test_broken_stream(self):
        initial_retry_test = {
            "instructions": {"storage.buckets.list": ["return-broken-stream"]}
        }
        request_body = json.dumps(initial_retry_test)
        self.conn.request("POST", "/retry_test", body=request_body)
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        retry_test = json.loads(body)
        retry_test_id = retry_test.get("id", None)
        self.assertIsNotNone(retry_test_id)
        self.conn.close()

        self.conn.request(
            "GET",
            "/storage/v1/b?project=test",
            headers={"x-retry-test-id": retry_test_id},
        )
        with self.assertRaises(http.client.IncompleteRead):
            response = self.conn.getresponse()
            response.read().decode("utf-8")
        self.conn.close()

        self.conn.request("DELETE", "/retry_test/%s" % (retry_test_id))
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        self.assertIn("Deleted %s" % (retry_test_id), body)
        self.conn.close()

    def test_retry_test_503(self):
        initial_retry_test = {
            "instructions": {"storage.buckets.list": ["return-503", "return-503"]}
        }
        request_body = json.dumps(initial_retry_test)
        self.conn.request("POST", "/retry_test", body=request_body)
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        retry_test = json.loads(body)
        retry_test_id = retry_test.get("id", None)
        self.assertIsNotNone(retry_test_id)
        self.conn.close()

        self.conn.request(
            "GET",
            "/storage/v1/b?project=test",
            headers={"x-retry-test-id": retry_test_id},
        )
        self.assertEqual(self.conn.getresponse().code, 503)
        self.conn.close()

        self.conn.request(
            "GET",
            "/storage/v1/b?project=test",
            headers={"x-retry-test-id": retry_test_id},
        )
        self.assertEqual(self.conn.getresponse().code, 503)
        self.conn.close()

        self.conn.request(
            "GET",
            "/storage/v1/b?project=test",
            headers={"x-retry-test-id": retry_test_id},
        )
        self.assertEqual(self.conn.getresponse().code, 200)
        self.conn.close()

        self.conn.request("DELETE", "/retry_test/%s" % (retry_test_id))
        response = self.conn.getresponse()
        body = response.read().decode("utf-8")
        self.assertIn("Deleted %s" % (retry_test_id), body)
        self.conn.close()

    def test_retry_test_malformed(self):
        invalid_retry_test_method = {
            "instructions": {"random_operation": ["returdn-503", "return-503"]}
        }
        request_body = json.dumps(invalid_retry_test_method)
        self.conn.request("POST", "/retry_test", body=request_body)
        response = self.conn.getresponse()
        self.assertEqual(response.code, 400)
        self.conn.close()

        invalid_retry_test_instruction = {
            "instructions": {"storage.buckets.list": ["ret-503"]}
        }
        request_body = json.dumps(invalid_retry_test_instruction)
        self.conn.request("POST", "/retry_test", body=request_body)
        response = self.conn.getresponse()
        self.assertEqual(response.code, 400)
        self.conn.close()


class TestHandleGzip(unittest.TestCase):
    def test_handle_decompressing(self):
        plain_text = b"hello world"
        compressed_text = gzip.compress(plain_text)
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=len(compressed_text),
            data=compressed_text,
            content_type="application/octet-stream",
            method="GET",
            headers={"Content-Encoding": "gzip"},
        )

        def passthrough_fn(environ, _):
            return environ

        middleware = utils.handle_gzip.HandleGzipMiddleware(passthrough_fn)
        decompressed_environ = middleware(environ, None)
        self.assertEqual(decompressed_environ["werkzeug.request"].data, plain_text)


if __name__ == "__main__":
    unittest.main()
