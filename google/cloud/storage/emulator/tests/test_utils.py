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

import pytest
import utils
from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2


class TestACL:
    def test_extract_predefined_default_object_acl(self):
        request = storage_pb2.InsertBucketRequest()
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, ""
        )
        assert predefined_default_object_acl == 0

        request.predefined_default_object_acl = 1
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, ""
        )
        assert predefined_default_object_acl == 1

        request = utils.common.FakeRequest(args={})
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, None
        )
        assert predefined_default_object_acl == ""

        request.args["predefinedDefaultObjectAcl"] = "authenticatedRead"
        predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
            request, None
        )
        assert predefined_default_object_acl == "authenticatedRead"

    def test_extract_predefined_acl(self):
        request = storage_pb2.InsertBucketRequest()
        predefined_acl = utils.acl.extract_predefined_acl(request, False, "")
        assert predefined_acl == 0

        request.predefined_acl = 1
        predefined_acl = utils.acl.extract_predefined_acl(request, False, "")
        assert predefined_acl == 1

        request = storage_pb2.CopyObjectRequest(destination_predefined_acl=2)
        predefined_acl = utils.acl.extract_predefined_acl(request, True, "")
        assert predefined_acl == 2

        request = utils.common.FakeRequest(args={})
        predefined_acl = utils.acl.extract_predefined_acl(request, False, None)
        assert predefined_acl == ""

        request.args["predefinedAcl"] = "authenticatedRead"
        predefined_acl = utils.acl.extract_predefined_acl(request, False, None)
        assert predefined_acl == "authenticatedRead"

        request.args["destinationPredefinedAcl"] = "bucketOwnerFullControl"
        predefined_acl = utils.acl.extract_predefined_acl(request, True, None)
        assert predefined_acl == "bucketOwnerFullControl"

    def test_compute_predefined_bucket_acl(self):
        acls = utils.acl.compute_predefined_bucket_acl(
            "bucket", "authenticatedRead", None
        )
        entities = [acl.entity for acl in acls]
        assert entities == [
            utils.acl.get_project_entity("owners", None),
            "allAuthenticatedUsers",
        ]

    def test_compute_predefined_default_object_acl(self):
        acls = utils.acl.compute_predefined_default_object_acl(
            "bucket", "authenticatedRead", None
        )
        entities = [acl.entity for acl in acls]
        assert entities == [
            utils.acl.get_object_entity("OWNER", None),
            "allAuthenticatedUsers",
        ]

        object_names = [acl.object for acl in acls]
        assert object_names == 2 * [""]

    def test_compute_predefined_object_acl(self):
        acls = utils.acl.compute_predefined_object_acl(
            "bucket", "object", 123456789, "authenticatedRead", None
        )
        entities = [acl.entity for acl in acls]
        assert entities == [
            utils.acl.get_object_entity("OWNER", None),
            "allAuthenticatedUsers",
        ]

        object_names = [acl.object for acl in acls]
        assert object_names == 2 * ["object"]

        generations = [acl.generation for acl in acls]
        assert generations == 2 * [123456789]


class TestCommonUtils:
    def test_snake_case(self):
        assert utils.common.to_snake_case("authenticatedRead") == "authenticated_read"
        assert (
            utils.common.to_snake_case("allAuthenticatedUsers")
            == "all_authenticated_users"
        )

    def test_parse_fields(self):
        fields = "kind, items ( acl( entity, role), name, id)"
        fields = fields.replace(" ", "")
        assert utils.common.parse_fields(fields) == [
            "kind",
            "items.acl.entity",
            "items.acl.role",
            "items.name",
            "items.id",
        ]

    def test_nested_key(self):
        doc = {
            "name": "bucket",
            "acl": [{"id": 1}, {"id": 2}],
            "labels": {"first": 1, "second": [1, 2]},
        }
        assert utils.common.nested_key(doc) == [
            "name",
            "acl[0].id",
            "acl[1].id",
            "labels.first",
            "labels.second[0]",
            "labels.second[1]",
        ]

    def test_extract_projection(self):
        request = storage_pb2.CopyObjectRequest()
        projection = utils.common.extract_projection(request, 1, "")
        assert projection == 1
        request.projection = 2
        projection = utils.common.extract_projection(request, 1, "")
        assert projection == 2

        request = utils.common.FakeRequest(args={})
        projection = utils.common.extract_projection(request, 1, None)
        assert projection == "noAcl"
        request.args["projection"] = "full"
        projection = utils.common.extract_projection(request, 1, None)
        assert projection == "full"


class TestGeneration:
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
        assert match is None
        assert not_match == 1
        match, not_match = utils.generation.extract_precondition(
            request, True, False, ""
        )
        assert match == 2
        assert not_match == 3
        match, not_match = utils.generation.extract_precondition(
            request, False, True, ""
        )
        assert match == 4
        assert not_match == 5
        match, not_match = utils.generation.extract_precondition(
            request, True, True, ""
        )
        assert match == 6
        assert not_match == 7

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
        assert match is None
        assert not_match == 1
        match, not_match = utils.generation.extract_precondition(
            request, True, False, None
        )
        assert match == 2
        assert not_match == 3
        match, not_match = utils.generation.extract_precondition(
            request, False, True, None
        )
        assert match == 4
        assert not_match == 5
        match, not_match = utils.generation.extract_precondition(
            request, True, True, None
        )
        assert match == 6
        assert not_match == 7

    def test_extract_generation(self):
        request = storage_pb2.GetObjectRequest()
        generation = utils.generation.extract_generation(request, False, "")
        assert generation == 0

        request.generation = 1
        generation = utils.generation.extract_generation(request, False, "")
        assert generation == 1

        request = storage_pb2.CopyObjectRequest(source_generation=2)
        generation = utils.generation.extract_generation(request, True, "")
        assert generation == 2

        request = utils.common.FakeRequest(args={})
        generation = utils.generation.extract_generation(request, False, None)
        assert generation == 0

        request.args["generation"] = 1
        request.args["sourceGeneration"] = 2
        generation = utils.generation.extract_generation(request, False, None)
        assert generation == 1
        generation = utils.generation.extract_generation(request, True, None)
        assert generation == 2


def run():
    pytest.main(["-v"])
