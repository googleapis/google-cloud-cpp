#!/usr/bin/env python
# Copyright 2018 Google Inc.
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
"""Implement a class to simulate GCS buckets."""


import base64
import error_response
import json
import testbench_utils
import time


class GcsBucket(object):
    """Represent a GCS Bucket."""

    def __init__(self, gcs_url, name):
        self.name = name
        self.gcs_url = gcs_url
        self.metadata = {
            'metageneration': 0,
            'name': self.name,
            'location': 'US',
            'storageClass': 'STANDARD',
            'etag': 'XYZ=',
            'labels': {
                'foo': 'bar',
                'baz': 'qux'
            },
            'owner': {
                'entity': 'project-owners-123456789',
                'entityId': '',
            }
        }
        self.notification_id = 1
        self.notifications = {}
        self.iam_version = 1
        self.iam_bindings = {}
        # Update the derived metadata attributes (e.g.: id, kind, selfLink)
        self.update_from_metadata({})
        self.insert_acl(
            testbench_utils.canonical_entity_name('project-owners-123456789'), 'OWNER')
        self.insert_acl(
            testbench_utils.canonical_entity_name('project-editors-123456789'), 'OWNER')
        self.insert_acl(
            testbench_utils.canonical_entity_name('project-viewers-123456789'), 'READER')
        self.insert_default_object_acl(
            testbench_utils.canonical_entity_name('project-owners-123456789'), 'OWNER')
        self.insert_default_object_acl(
            testbench_utils.canonical_entity_name('project-editors-123456789'), 'OWNER')
        self.insert_default_object_acl(
            testbench_utils.canonical_entity_name('project-viewers-123456789'), 'READER')

    def increase_metageneration(self):
        """Increase the current metageneration number."""
        new = self.metadata.get('metageneration', 0) + 1
        self.metadata['metageneration'] = new

    def versioning_enabled(self):
        """Return True if versioning is enabled for this Bucket."""
        v = self.metadata.get('versioning', None)
        if v is None:
            return False
        return v.get('enabled', False)

    @classmethod
    def _remove_non_writable_keys(cls, metadata):
        """Remove the keys from metadata (an update or patch) that are not
         writable.

         Both `Buckets: patch` and `Buckets: update` either ignore non-writable
         keys or return 400 if the key does not match the current value. In
         the testbench we simply always ignore them, to make life easier.

         :param metadata:dict a dictionary representing a patch or
             update to the metadata.
         :return metadata but with only any non-writable keys removed.
         :rtype: dict
         """
        writeable_keys = {
            'acl', 'billing', 'cors', 'defaultObjectAcl', 'encryption',
            'labels', 'lifecycle', 'location', 'logging', 'storageClass',
            'versioning', 'website'
        }
        for key in metadata.keys():
            if key not in writeable_keys:
                metadata.pop(key, None)
        return metadata

    def update_from_metadata(self, metadata):
        """Update from a metadata dictionary.

        :param metadata:dict a dictionary with new metadata values.
        """
        retention_policy = metadata.get('retentionPolicy')
        if retention_policy:
            # Ignore any values set for 'isLocked' or 'effectiveTime'.
            retention_policy.pop('isLocked', None)
            now = time.gmtime(time.time())
            timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
            retention_policy['effectiveTime'] = timestamp
            metadata['retentionPolicy'] = retention_policy
        tmp = self.metadata.copy()
        metadata = GcsBucket._remove_non_writable_keys(metadata)
        tmp.update(metadata)
        tmp['name'] = tmp.get('name', self.name)
        tmp.update({
            'id': self.name,
            'kind': 'storage#bucket',
            'selfLink': self.gcs_url + self.name,
            'projectNumber': '123456789',
            'timeCreated': '2018-05-19T19:31:14Z',
            'updated': '2018-05-19T19:31:24Z',
        })
        self.metadata = tmp
        self.increase_metageneration()

    def apply_patch(self, patch):
        """Update from a metadata dictionary.

        :param patch:dict a dictionary with metadata changes.
        """
        patch = GcsBucket._remove_non_writable_keys(patch)
        retention_policy = patch.get('retentionPolicy')
        if retention_policy:
            # Ignore any values set for 'isLocked' or 'effectiveTime'.
            retention_policy.pop('isLocked', None)
            now = time.gmtime(time.time())
            timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
            retention_policy['effectiveTime'] = timestamp
            patch['retentionPolicy'] = retention_policy
        patched = testbench_utils.json_api_patch(self.metadata, patch, recurse_on={'labels'})
        self.metadata = patched
        self.increase_metageneration()

    def check_preconditions(self, request):
        """Verify that the preconditions in request are met.

        :param request:flask.Request the contents of the HTTP request.
        :rtype:NoneType
        :raises:ErrorResponse if the request does not pass the preconditions,
            for example, the request has a `ifMetagenerationMatch` restriction
            that is not met.
        """

        metageneration_match = request.args.get('ifMetagenerationMatch')
        metageneration_not_match = request.args.get('ifMetagenerationNotMatch')
        metageneration = self.metadata.get('metageneration')

        if (metageneration_not_match is not None
                and int(metageneration_not_match) == metageneration):
            raise error_response.ErrorResponse(
                'Precondition Failed (metageneration = %s)' % metageneration,
                status_code=412)

        if (metageneration_match is not None
                and int(metageneration_match) != metageneration):
            raise error_response.ErrorResponse(
                'Precondition Failed (metageneration = %s)' % metageneration,
                status_code=412)

    def create_acl_entry(self, entity, role):
        """Return an ACL entry for the given entity and role.

        :param entity: str the user, group or email granted permissions.
        :param role: str the name of the permissions (READER, WRITER, OWNER).
        :return: the canonical entity name and the ACL entry.
        :rtype: (str,dict)
        """
        entity = testbench_utils.canonical_entity_name(entity)
        email = ''
        if entity.startswith('user-'):
            email = entity.replace('user-', '', 1)
        return (entity, {
            'bucket': self.name,
            'email': email,
            'entity': entity,
            'etag': self.metadata.get('etag', 'XYZ='),
            'id': self.metadata.get('id', '') + '/' + entity,
            'kind': 'storage#bucketAccessControl',
            'role': role,
            'selfLink': self.metadata.get('selfLink') + '/acl/' + entity
        })

    def insert_acl(self, entity, role):
        """Insert (or update) a new BucketAccessControl entry for this bucket.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return: the dictionary representing the new AccessControl metadata.
        :rtype: dict
        """
        entity, entry = self.create_acl_entry(entity, role)
        # Replace or insert the entry.
        indexed = testbench_utils.index_acl(self.metadata.get('acl', []))
        indexed[entity] = entry
        self.metadata['acl'] = indexed.values()
        return entry

    def delete_acl(self, entity):
        """
        Delete a single BucketAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :rtype:NoneType
        """
        entity = testbench_utils.canonical_entity_name(entity)
        indexed = testbench_utils.index_acl(self.metadata.get('acl', []))
        indexed.pop(entity)
        self.metadata['acl'] = indexed.values()

    def get_acl(self, entity):
        """Get a single BucketAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :return: with the contents of the BucketAccessControl.
        :rtype: dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        for acl in self.metadata.get('acl', []):
            if acl.get('entity', '') == entity:
                return acl
        raise error_response.ErrorResponse(
            'Entity %s not found in object %s' % (entity, self.name))

    def update_acl(self, entity, role):
        """Update a single BucketAccessControl entry in this bucket.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return: with the contents of the BucketAccessControl.
        :rtype: dict
        """
        return self.insert_acl(entity, role)

    def insert_default_object_acl(self, entity, role):
        """Insert (or update) a new default ObjectAccessControl entry for this
        bucket.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return: the dictionary representing the new ObjectAccessControl.
        :rtype: dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        email = ''
        if entity.startswith('user-'):
            email = email.replace('user-', '', 1)
        # Replace or insert the entry.
        indexed = testbench_utils.index_acl(self.metadata.get('defaultObjectAcl', []))
        indexed[entity] = {
            'bucket': self.name,
            'email': email,
            'entity': entity,
            'etag': self.metadata.get('etag', 'XYZ='),
            'id': self.metadata.get('id', '') + '/' + entity,
            'kind': 'storage#objectAccessControl',
            'role': role,
            'selfLink': self.metadata.get('selfLink') + '/acl/' + entity
        }
        self.metadata['defaultObjectAcl'] = indexed.values()
        return indexed[entity]

    def delete_default_object_acl(self, entity):
        """Delete a single default ObjectAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :rtype:NoneType
        """
        entity = testbench_utils.canonical_entity_name(entity)
        indexed = testbench_utils.index_acl(self.metadata.get('defaultObjectAcl', []))
        indexed.pop(entity)
        self.metadata['defaultObjectAcl'] = indexed.values()

    def get_default_object_acl(self, entity):
        """Get a single default ObjectAccessControl entry from this Bucket.

        :param entity:str the name of the entity.
        :return: with the contents of the BucketAccessControl.
        :rtype: dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        for acl in self.metadata.get('defaultObjectAcl', []):
            if acl.get('entity', '') == entity:
                return acl
        raise error_response.ErrorResponse(
            'Entity %s not found in object %s' % (entity, self.name))

    def update_default_object_acl(self, entity, role):
        """Update a single default ObjectAccessControl entry in this Bucket.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return: with the contents of the ObjectAccessControl.
        :rtype: dict
        """
        return self.insert_default_object_acl(entity, role)

    def list_notifications(self):
        """List the notifications associated with this Bucket.

        :return: with the notification definitions.
        :rtype: list[dict]
        """
        return self.notifications.values()

    def insert_notification(self, request):
        """
        Insert a new notification into this Bucket.

        :param request:flask.Request the HTTP request contents.
        :return: the new notification value.
        :rtype:dict
        """
        notification_id = 'notification-%d' % self.notification_id
        link = '%s/b/%s/notificationConfigs/%s' % (self.gcs_url, self.name,
                                                   notification_id)
        self.notification_id += 1
        notification = json.loads(request.data)
        notification.update({
            'id': notification_id,
            'selfLink': link,
            'etag': 'XYZ=',
            'kind': 'storage#notification',
        })
        self.notifications[notification_id] = notification
        return notification

    def delete_notification(self, notification_id):
        """Delete a notification in this Bucket.

        :param notification_id:str the id of the notification.
        :rtype:NoneType
        """
        if self.notifications.get(notification_id) is None:
            raise error_response.ErrorResponse(
                'Notification %d not found in %s' % (notification_id,
                                                     self.name),
                status_code=404)
        del (self.notifications[notification_id])

    def get_notification(self, notification_id):
        """
        Get the details of a given notification in this Bucket.

        :param notification_id:str the id of the notification.
        :return: the details of the notification.
        :rtype: dict
        """
        details = self.notifications.get(notification_id)
        if details is None:
            raise error_response.ErrorResponse(
                'Notification %d not found in %s' % (notification_id,
                                                     self.name),
                status_code=404)
        return details

    def iam_policy_as_json(self):
        """Get the current IamPolicy in the right format for JSON."""
        role_mapping = {
            'READER': 'roles/storage.legacyBucketReader',
            'WRITER': 'roles/storage.legacyBucketWriter',
            'OWNER': 'roles/storage.legacyBucketOwner',
        }
        members_by_role = self.iam_bindings.copy()
        if self.metadata.get('acl') is not None:
            # Store the ACLs as IamBindings
            for entry in self.metadata.get('acl', []):
                legacy_role = entry.get('role')
                if legacy_role is None or entry.get('entity') is None:
                    raise error_response.ErrorResponse(
                        'Invalid ACL entry', status_code=500)
                role = role_mapping.get(legacy_role)
                if role is None:
                    raise error_response.ErrorResponse(
                        'Invalid legacy role %s' % legacy_role,
                        status_code=500)
                members_by_role.setdefault(role, []).append(
                    entry.get('entity'))
        bindings = []
        for k, v in members_by_role.iteritems():
            bindings.append({'role': k, 'members': v})
        policy = {
            'kind': 'storage#policy',
            'resourceId': 'projects/_/buckets/%s' % self.name,
            'bindings': bindings,
            'etag': base64.b64encode(str(self.iam_version))
        }
        return policy

    def get_iam_policy(self, request):
        """Get the IamPolicy associated with this Bucket.

        :param request: flask.Request the http request.
        :return: the IamPolicy as a dictionary, ready for JSON encoding.
        :rtype: dict
        """
        self.check_preconditions(request)
        return self.iam_policy_as_json()

    def set_iam_policy(self, request):
        """Set the IamPolicy associated with this Bucket.

        :param request: flask.Request the original http request.
        :return: the IamPolicy as a dictionary, ready for JSON encoding.
        :rtype: dict
        """
        self.check_preconditions(request)
        current_etag = base64.b64encode(str(self.iam_version))
        if (request.headers.get('if-match') is not None
                and current_etag != request.headers.get('if-match')):
            raise error_response.ErrorResponse(
                'Mismatched ETag has %s' % current_etag, status_code=412)
        if (request.headers.get('if-none-match') is not None
                and current_etag == request.headers.get('if-none-match')):
            raise error_response.ErrorResponse(
                'Mismatched ETag has %s' % current_etag, status_code=412)

        policy = json.loads(request.data)
        if policy.get('bindings') is None:
            raise error_response.ErrorResponse('Missing "bindings" field')

        new_acl = []
        new_iam_bindings = {}
        role_mapping = {
            'roles/storage.legacyBucketReader': 'READER',
            'roles/storage.legacyBucketWriter': 'WRITER',
            'roles/storage.legacyBucketOwner': 'OWNER'
        }
        for binding in policy.get('bindings'):
            role = binding.get('role')
            members = binding.get('members')
            if role is None or members is None:
                raise error_response.ErrorResponse(
                    'Missing "role" or "members" fields')
            if role_mapping.get(role) is None:
                new_iam_bindings[role] = members
            else:
                for m in members:
                    legacy_role = role_mapping.get(role)
                    _, entry = self.create_acl_entry(
                        entity=m, role=legacy_role)
                    new_acl.append(entry)
        self.metadata['acl'] = new_acl
        self.iam_bindings = new_iam_bindings
        self.iam_version = self.iam_version + 1
        return self.iam_policy_as_json()

    def test_iam_permissions(self, request):
        """Test the IAM permissions for the current user.

        Because we do not want to implement a full simulator for IAM, we simply
        return the permissions matching 'storage.*'

        :param request: flask.Request the current http request.
        :return: formatted for `Buckets: testIamPermissions`
        :rtype: dict
        """
        result = {
            'kind': 'storage#testIamPermissionsResponse',
            'permissions': []
        }
        for p in request.args.getlist('permissions'):
            if p.startswith('storage.'):
                result['permissions'].append(p)
        return result
