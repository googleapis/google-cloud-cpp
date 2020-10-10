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

"""Utils related to access control"""

import hashlib
import os

import utils
from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2

PROJECT_NUMBER = os.getenv(
    "GOOGLE_CLOUD_CPP_STORAGE_EMULATOR_PROJECT_NUMBER", "123456789"
)
OBJECT_OWNER_ENTITY = os.getenv(
    "GOOGLE_CLOUD_CPP_STORAGE_EMULATOR_OBJECT_OWNER_ENTITY",
    "user-object.owners@example.com",
)
OBJECT_READER_ENTITY = os.getenv(
    "GOOGLE_CLOUD_CPP_STORAGE_EMULATOR_OBJECT_READER_ENTITY",
    "user-object.viewers@example.com",
)


# === EXTRACT INFORMATION FROM ENTITY === #


def __extract_email(entity):
    if entity.startswith("user-"):
        return entity[len("user-") :]
    elif entity.startswith("group-") and "@" in entity:
        return entity[len("group-") :]
    return ""


def __extract_domain(entity):
    if entity.startswith("domain-"):
        return entity[len("domain-") :]
    return ""


def __extract_team(entity):
    if entity.startswith("project-"):
        return entity.split("-")[1]
    return ""


# === ENTITY UTILS === #


def get_canonical_entity(entity):
    if entity == "allUsers" or entity == "allAuthenticatedUsers":
        return entity
    if entity.startswith("project-owners-"):
        entity = "project-owners-" + PROJECT_NUMBER
    if entity.startswith("project-editors-"):
        entity = "project-editors-" + PROJECT_NUMBER
    if entity.startswith("project-viewers-"):
        entity = "project-viewers-" + PROJECT_NUMBER
    return entity.lower()


def get_project_entity(team, context):
    if team not in ["editors", "owners", "viewers"]:
        utils.error.invaild("Team %s for project" % team, context)
    return "project-%s-%s" % (team, PROJECT_NUMBER)


def get_object_entity(role, context):
    if role == "OWNER":
        return OBJECT_OWNER_ENTITY
    elif role == "READER":
        return OBJECT_READER_ENTITY
    else:
        utils.error.invaild("Role %s for object acl" % role, context)


# === CREATE ACL === #


def create_bucket_acl(bucket_name, entity, role, context):
    entity = get_canonical_entity(entity)
    if role not in ["OWNER", "WRITER", "READER"]:
        utils.error.invaild("Role %s for bucket acl" % role, context)
    etag = hashlib.md5((bucket_name + entity + role).encode("utf-8")).hexdigest()
    acl = resources_pb2.BucketAccessControl(
        role=role,
        etag=etag,
        id=etag,
        bucket=bucket_name,
        entity=entity,
        entity_id=hashlib.md5(entity.encode("utf-8")).hexdigest(),
        email=__extract_email(entity),
        domain=__extract_domain(entity),
        project_team={"project_number": PROJECT_NUMBER, "team": __extract_team(entity)},
    )
    return acl


def create_default_object_acl(bucket_name, entity, role, context):
    entity = get_canonical_entity(entity)
    if role not in ["OWNER", "READER"]:
        utils.error.invaild("Role %s for object acl" % role, context)
    etag = hashlib.md5(
        (bucket_name + entity + role + "storage#objectAccessControl").encode("utf-8")
    ).hexdigest()
    acl = resources_pb2.ObjectAccessControl(
        role=role,
        etag=etag,
        bucket=bucket_name,
        entity=entity,
        entity_id=hashlib.md5(entity.encode("utf-8")).hexdigest(),
        email=__extract_email(entity),
        domain=__extract_domain(entity),
        project_team={"project_number": PROJECT_NUMBER, "team": __extract_team(entity)},
    )
    return acl


def create_object_acl_from_default_object_acl(
    object_name, generation, default_object_acl, context
):
    acl = resources_pb2.ObjectAccessControl()
    acl.CopyFrom(default_object_acl)
    acl.id = hashlib.md5(
        (acl.bucket + object_name + str(generation) + acl.entity + acl.role).encode(
            "utf-8"
        )
    ).hexdigest()
    acl.etag = acl.id
    acl.object = object_name
    acl.generation = generation
    return acl


def create_object_acl(bucket_name, object_name, generation, entity, role, context):
    entity = get_canonical_entity(entity)
    default_object_acl = create_default_object_acl(bucket_name, entity, role, context)
    acl = create_object_acl_from_default_object_acl(
        object_name, generation, default_object_acl, context
    )
    return acl


# === EXTRACT INFORMATION FROM REQUEST === #


def extract_predefined_acl(request, is_destination, context):
    if context is not None:
        extract_field = (
            "predefined_acl" if not is_destination else "destination_predefined_acl"
        )
        return getattr(request, extract_field, None)
    else:
        extract_field = (
            "predefinedAcl" if not is_destination else "destinationPredefinedAcl"
        )
        return request.args.get(extract_field, "")


def extract_predefined_default_object_acl(request, context):
    return (
        request.args.get("predefinedDefaultObjectAcl", "")
        if context is None
        else request.predefined_default_object_acl
    )


# === COMPUTE PREDEFINED ACL === #

predefined_bucket_acl_map = {
    "authenticated_read": 1,
    "private": 2,
    "project_private": 3,
    "public_read": 4,
    "public_read_write": 5,
}


def compute_predefined_bucket_acl(bucket_name, predefined_acl, context):
    if context is None:
        predefined_acl = utils.common.to_snake_case(predefined_acl)
        predefined_acl = predefined_bucket_acl_map.get(predefined_acl)
        if predefined_acl is None:
            return []
    acls = []
    if predefined_acl == 1:
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
        acls.append(
            create_bucket_acl(bucket_name, "allAuthenticatedUsers", "READER", context)
        )
    elif predefined_acl == 2:
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
    elif predefined_acl == 3:
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("editors", context), "WRITER", context
            )
        )
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("viewers", context), "READER", context
            )
        )
    elif predefined_acl == 4:
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
        acls.append(create_bucket_acl(bucket_name, "allUsers", "READER", context))
    elif predefined_acl == 5:
        acls.append(
            create_bucket_acl(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
        acls.append(create_bucket_acl(bucket_name, "allUsers", "WRITER", context))
    return acls


predefined_object_acl_map = {
    "authenticated_read": 1,
    "bucket_owner_full_control": 2,
    "bucket_owner_read": 3,
    "private": 4,
    "project_private": 5,
    "public_read": 6,
}


def __compute_predefined_object_acl(bucket_name, predefined_acl, acl_factory, context):
    if context is None:
        predefined_acl = utils.common.to_snake_case(predefined_acl)
        predefined_acl = predefined_object_acl_map.get(predefined_acl)
        if predefined_acl is None:
            return []
    acls = []
    if predefined_acl == 1:
        acls.append(
            acl_factory(
                bucket_name, get_object_entity("OWNER", context), "OWNER", context
            )
        )
        acls.append(
            acl_factory(bucket_name, "allAuthenticatedUsers", "READER", context)
        )
    elif predefined_acl == 2:
        acls.append(
            acl_factory(
                bucket_name, get_object_entity("OWNER", context), "OWNER", context
            )
        )
        acls.append(
            acl_factory(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
    elif predefined_acl == 3:
        acls.append(
            acl_factory(
                bucket_name, get_object_entity("OWNER", context), "OWNER", context
            )
        )
        acls.append(
            acl_factory(
                bucket_name, get_project_entity("owners", context), "READER", context
            )
        )
    elif predefined_acl == 4:
        acls.append(
            acl_factory(
                bucket_name, get_object_entity("OWNER", context), "OWNER", context
            )
        )
    elif predefined_acl == 5:
        acls.append(
            acl_factory(
                bucket_name, get_object_entity("OWNER", context), "OWNER", context
            )
        )
        acls.append(
            acl_factory(
                bucket_name, get_project_entity("owners", context), "OWNER", context
            )
        )
        acls.append(
            acl_factory(
                bucket_name, get_project_entity("editors", context), "OWNER", context
            )
        )
        acls.append(
            acl_factory(
                bucket_name, get_project_entity("viewers", context), "READER", context
            )
        )
    elif predefined_acl == 6:
        acls.append(
            acl_factory(
                bucket_name, get_object_entity("OWNER", context), "OWNER", context
            )
        )
        acls.append(acl_factory(bucket_name, "allUsers", "READER", context))
    return acls


def compute_predefined_default_object_acl(
    bucket_name, predefined_default_object_acl, context
):
    return __compute_predefined_object_acl(
        bucket_name, predefined_default_object_acl, create_default_object_acl, context
    )


def compute_predefined_object_acl(
    bucket_name, object_name, generation, predefined_acl, context
):
    def object_acl_factory(bucket_name, entity, role, context):
        return create_object_acl(
            bucket_name, object_name, generation, entity, role, context
        )

    return __compute_predefined_object_acl(
        bucket_name, predefined_acl, object_acl_factory, context
    )
