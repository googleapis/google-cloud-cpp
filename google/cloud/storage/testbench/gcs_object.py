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
"""Implement a class to simulate GCS objects."""


import base64
import error_response
import hashlib
import json
import testbench_utils
import time


class GcsObjectVersion(object):
    """Represent a single revision of a GCS Object."""

    def __init__(self, gcs_url, bucket_name, name, generation, request, media):
        """Initialize a new object revision.

        :param gcs_url:str the base URL for the GCS service.
        :param bucket_name:str the name of the bucket that contains the object.
        :param name:str the name of the object.
        :param generation:int the generation number for this object.
        :param request:flask.Request the contents of the HTTP request.
        :param media:str the contents of the object.
        """
        self.gcs_url = gcs_url
        self.bucket_name = bucket_name
        self.name = name
        self.generation = generation
        self.object_id = bucket_name + '/o/' + name + '/' + str(generation)
        now = time.gmtime(time.time())
        timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
        self.media = media
        instructions = request.headers.get('x-goog-testbench-instructions')
        if instructions == 'inject-upload-data-error':
            self.media = testbench_utils.corrupt_media(media)

        self.metadata = {
            'timeCreated': timestamp,
            'updated': timestamp,
            'metageneration': 0,
            'generation': generation,
            'location': 'US',
            'storageClass': 'STANDARD',
            'size': len(self.media),
            'etag': 'XYZ=',
            'owner': {
                'entity': 'project-owners-123456789',
                'entityId': '',
            },
            'md5Hash': base64.b64encode(hashlib.md5(self.media).digest()),
        }
        if request.headers.get('content-type') is not None:
            self.metadata['contentType'] = request.headers.get('content-type')
        # Update the derived metadata attributes (e.g.: id, kind, selfLink)
        self.update_from_metadata({})
        # Capture any encryption key headers.
        self._capture_customer_encryption(request)
        self._update_predefined_acl(request.args.get('predefinedAcl'))
        acl2json_mapping = {
            'authenticated-read': 'authenticatedRead',
            'bucket-owner-full-control': 'bucketOwnerFullControl',
            'bucket-owner-read': 'bucketOwnerRead',
            'private': 'private',
            'project-private': 'projectPrivate',
            'public-read': 'publicRead',
        }
        if request.headers.get('x-goog-acl') is not None:
            acl = request.headers.get('x-goog-acl')
            predefined = acl2json_mapping.get(acl)
            if predefined is not None:
                self._update_predefined_acl(predefined)
            else:
                raise error_response.ErrorResponse(
                    'Invalid predefinedAcl value %s' % acl, status_code=400)

    def update_from_metadata(self, metadata):
        """Update from a metadata dictionary.

        :param metadata:dict a dictionary with new metadata values.
        :rtype:NoneType
        """
        tmp = self.metadata.copy()
        tmp.update(metadata)
        tmp['bucket'] = tmp.get('bucket', self.name)
        tmp['name'] = tmp.get('name', self.name)
        now = time.gmtime(time.time())
        timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
        # Some values cannot be changed via updates, so we always reset them.
        tmp.update({
            'kind': 'storage#object',
            'bucket': self.bucket_name,
            'name': self.name,
            'id': self.object_id,
            'selfLink': self.gcs_url + self.name,
            'projectNumber': '123456789',
            'updated': timestamp,
        })
        tmp['metageneration'] = tmp.get('metageneration', 0) + 1
        self.metadata = tmp
        self._validate_hashes()

    def _validate_hashes(self):
        """Validate the md5Hash field against the stored media."""
        actual = self.metadata.get('md5Hash', '')
        expected = base64.b64encode(hashlib.md5(self.media).digest())
        if actual != expected:
            raise error_response.ErrorResponse(
                'Mismatched MD5 hash expected=%s, actual=%s' % (expected,
                                                                actual))

    def validate_encryption_for_read(self, request,
                                     prefix='x-goog-encryption'):
        """Verify that the request includes the correct encryption keys.

        :param request:flask.Request the http request.
        :param prefix: str the prefix shared by the encryption headers,
            typically 'x-goog-encryption', but for rewrite requests it can be
            'x-good-copy-source-encryption'.
        :rtype:NoneType
        """
        key_header = prefix + '-key'
        hash_header = prefix + '-key-sha256'
        algo_header = prefix + '-algorithm'
        encryption = self.metadata.get('customerEncryption')
        if encryption is None:
            # The object is not encrypted, no key is needed.
            if request.headers.get(key_header) is None:
                return
            else:
                # The data is not encrypted, sending an encryption key is an
                # error.
                testbench_utils.raise_csek_error()
        # The data is encrypted, the key must be present, match, and match its
        # hash.
        key_header_value = request.headers.get(key_header)
        hash_header_value = request.headers.get(hash_header)
        algo_header_value = request.headers.get(algo_header)
        testbench_utils.validate_customer_encryption_headers(
            key_header_value, hash_header_value, algo_header_value)
        if encryption.get('keySha256') != hash_header_value:
            testbench_utils.raise_csek_error()

    def _capture_customer_encryption(self, request):
        """Capture the customer-supplied encryption key, if any.

        :param request:flask.Request the http request.
        :rtype:NoneType
        """
        if request.headers.get('x-goog-encryption-key') is None:
            return
        prefix = 'x-goog-encryption'
        key_header = prefix + '-key'
        hash_header = prefix + '-key-sha256'
        algo_header = prefix + '-algorithm'
        key_header_value = request.headers.get(key_header)
        hash_header_value = request.headers.get(hash_header)
        algo_header_value = request.headers.get(algo_header)
        testbench_utils.validate_customer_encryption_headers(
            key_header_value, hash_header_value, algo_header_value)
        self.metadata['customerEncryption'] = {
            "encryptionAlgorithm": algo_header_value,
            "keySha256": hash_header_value,
        }

    def _update_predefined_acl(self, predefined_acl):
        """Update the ACL based on the given request parameter value."""
        if predefined_acl is None:
            predefined_acl = 'projectPrivate'
        self.insert_acl(
            testbench_utils.canonical_entity_name('project-owners-123456789'), 'OWNER')
        bucket = testbench_utils.lookup_bucket(self.bucket_name)
        owner = bucket.metadata.get('owner')
        if owner is None:
            owner_entity = 'project-owners-123456789'
        else:
            owner_entity = owner.get('entity')
        if predefined_acl == 'authenticatedRead':
            self.insert_acl('allAuthenticatedUsers', 'READER')
        elif predefined_acl == 'bucketOwnerFullControl':
            self.insert_acl(owner_entity, 'OWNER')
        elif predefined_acl == 'bucketOwnerRead':
            self.insert_acl(owner_entity, 'READER')
        elif predefined_acl == 'private':
            self.insert_acl('project-owners', 'OWNER')
        elif predefined_acl == 'projectPrivate':
            self.insert_acl(
                testbench_utils.canonical_entity_name('project-editors-123456789'), 'OWNER')
            self.insert_acl(
                testbench_utils.canonical_entity_name('project-viewers-123456789'), 'READER')
        elif predefined_acl == 'publicRead':
            self.insert_acl(
                testbench_utils.canonical_entity_name('allUsers'), 'READER')
        else:
            raise error_response.ErrorResponse(
                'Invalid predefinedAcl value', status_code=400)

    def reset_predefined_acl(self, predefined_acl):
        """Reset the ACL based on the given request parameter value."""
        self.metadata['acl'] = []
        self._update_predefined_acl(predefined_acl)

    def insert_acl(self, entity, role):
        """Insert (or update) a new AccessControl entry for this object.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return: the dictionary representing the new AccessControl metadata.
        :rtype:dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        email = ''
        if entity.startswith('user-'):
            email = entity
        # Replace or insert the entry.
        indexed = testbench_utils.index_acl(self.metadata.get('acl', []))
        indexed[entity] = {
            'bucket': self.bucket_name,
            'email': email,
            'entity': entity,
            'entity_id': '',
            'etag': self.metadata.get('etag', 'XYZ='),
            'generation': self.generation,
            'id': self.metadata.get('id', '') + '/' + entity,
            'kind': 'storage#objectAccessControl',
            'object': self.name,
            'role': role,
            'selfLink': self.metadata.get('selfLink') + '/acl/' + entity
        }
        self.metadata['acl'] = indexed.values()
        return indexed[entity]

    def delete_acl(self, entity):
        """Delete a single AccessControl entry from the Object revision.

        :param entity:str the name of the entity.
        :rtype:NoneType
        """
        entity = testbench_utils.canonical_entity_name(entity)
        indexed = testbench_utils.index_acl(self.metadata.get('acl', []))
        indexed.pop(entity)
        self.metadata['acl'] = indexed.values()

    def get_acl(self, entity):
        """Get a single AccessControl entry from the Object revision.

        :param entity:str the name of the entity.
        :return: with the contents of the ObjectAccessControl.
        :rtype:dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        for acl in self.metadata.get('acl', []):
            if acl.get('entity', '') == entity:
                return acl
        raise error_response.ErrorResponse(
            'Entity %s not found in object %s' % (entity, self.name))

    def update_acl(self, entity, role):
        """Update a single AccessControl entry in this Object revision.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return: with the contents of the ObjectAccessControl.
        :rtype: dict
        """
        return self.insert_acl(entity, role)

    def patch_acl(self, entity, request):
        """Patch a single AccessControl entry in this Object revision.

        :param entity:str the name of the entity.
        :param request:flask.Request the parameters for this request.
        :return: with the contents of the ObjectAccessControl.
        :rtype: dict
        """
        acl = self.get_acl(entity)
        payload = json.loads(request.data)
        request_entity = payload.get('entity')
        if request_entity is not None and request_entity != entity:
            raise error_response.ErrorResponse(
                'Entity mismatch in ObjectAccessControls: patch, expected=%s, got=%s'
                % (entity, request_entity))
        etag_match = request.headers.get('if-match')
        if etag_match is not None and etag_match != acl.get('etag'):
            raise error_response.ErrorResponse(
                'Precondition Failed', status_code=412)
        etag_none_match = request.headers.get('if-none-match')
        if (etag_none_match is not None
                and etag_none_match != acl.get('etag')):
            raise error_response.ErrorResponse(
                'Precondition Failed', status_code=412)
        role = payload.get('role')
        if role is None:
            raise error_response.ErrorResponse('Missing role value')
        return self.insert_acl(entity, role)
