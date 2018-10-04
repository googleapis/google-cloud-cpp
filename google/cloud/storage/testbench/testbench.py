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
"""A test bench for the Google Cloud Storage C++ Client Library."""

import argparse
import base64
import error_response
import flask
import hashlib
import httpbin
import json
import os
import re
import testbench_utils
import time
from werkzeug import serving
from werkzeug import wsgi


@httpbin.app.errorhandler(error_response.ErrorResponse)
def httpbin_error(error):
    return error.as_response()


root = flask.Flask(__name__)
root.debug = True


@root.route('/')
def index():
    """Default handler for the test bench."""
    return 'OK'


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
        bucket = lookup_bucket(self.bucket_name)
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


class GcsObject(object):
    """Represent a GCS Object, including all its revisions."""

    def __init__(self, bucket_name, name):
        """Initialize a fake GCS Blob.

        :param bucket_name:str the bucket that will contain the new object.
        :param name:str the name of the new object.
        """
        self.bucket_name = bucket_name
        self.name = name
        # Define the current generation for the object, will use this as a
        # simple counter to increment on each object change.
        self.generation = 0
        self.revisions = {}
        self.rewrite_token_generator = 0
        self.rewrite_operations = {}

    def get_revision(self, request, version_field_name='generation'):
        """Get the information about a particular object revision or raise.

        :param request:flask.Request the contents of the http request.
        :param version_field_name:str the name of the generation
            parameter, typically 'generation', but sometimes 'sourceGeneration'.
        :return: the object revision.
        :rtype: GcsObjectVersion
        :raises:ErrorResponse if the request contains an invalid generation
            number.
        """
        generation = request.args.get(version_field_name)
        if generation is None:
            return self.get_latest()
        version = self.revisions.get(int(generation))
        if version is None:
            raise error_response.ErrorResponse(
                'Precondition Failed: generation %s not found' % generation)
        return version

    def del_revision(self, request):
        """Delete a version of a fake GCS Blob.

        :param request:flask.Request the contents of the HTTP request.
        :return: True if the object entry in the Bucket should be deleted.
        :rtype: bool
        """
        generation = request.args.get('generation')
        if generation is None:
            generation = self.generation
        self.revisions.pop(int(generation))
        if len(self.revisions) == 0:
            self.generation = None
            return True
        if generation == self.generation:
            self.generation = sorted(self.revisions.keys())[-1]
        return False

    def update_revision(self, request):
        """Update the metadata of particular object revision or raise.

        :param request:flask.Request
        :return: the object revision updated revision.
        :rtype:GcsObjectVersion
        :raises:ErrorResponse if the request contains an invalid generation
            number.
        """
        generation = request.args.get('generation')
        if generation is None:
            version = self.get_latest()
        else:
            version = self.revisions.get(int(generation))
            if version is None:
                raise error_response.ErrorResponse(
                    'Precondition Failed: generation %s not found' %
                    generation)
        metadata = json.loads(request.data)
        version.update_from_metadata(metadata)
        return version

    def patch_revision(self, request):
        """Patch the metadata of particular object revision or raise.

        :param request:flask.Request
        :return: the object revision.
        :rtype:GcsObjectRevision
        :raises:ErrorResponse if the request contains an invalid generation
            number.
        """
        generation = request.args.get('generation')
        if generation is None:
            version = self.get_latest()
        else:
            version = self.revisions.get(int(generation))
            if version is None:
                raise error_response.ErrorResponse(
                    'Precondition Failed: generation %s not found' %
                    generation)
        patch = json.loads(request.data)
        writeable_keys = {
            'acl', 'cacheControl', 'contentDisposition', 'contentEncoding',
            'contentLanguage', 'contentType', 'metadata'
        }
        for key, value in patch.iteritems():
            if key not in writeable_keys:
                raise error_response.ErrorResponse(
                    'Invalid metadata change. %s is not writeable' % key,
                    status_code=503)
        patched = testbench_utils.json_api_patch(
            version.metadata, patch, recurse_on={'metadata'})
        patched['metageneration'] = patched.get('metageneration', 0) + 1
        version.metadata = patched
        return version

    def get_revision_by_generation(self, generation):
        """Get object revision by generation or None if not found.

        :param generation:int
        :return: the object revision by generation or None.
        :rtype:GcsObjectRevision
        """
        return self.revisions.get(generation, None)

    def get_latest(self):
        return self.revisions.get(self.generation, None)

    def check_preconditions_by_value(
            self, generation_match, generation_not_match, metageneration_match,
            metageneration_not_match):
        """Verify that the given precondition values are met."""
        if (generation_match is not None
                and int(generation_match) != self.generation):
            raise error_response.ErrorResponse(
                'Precondition Failed', status_code=412)
        # This object does not exist (yet), testing in this case is special.
        if (generation_not_match is not None
                and int(generation_not_match) == self.generation):
            raise error_response.ErrorResponse(
                'Precondition Failed', status_code=412)

        if self.generation == 0:
            if (metageneration_match is not None
                    or metageneration_not_match is not None):
                raise error_response.ErrorResponse(
                    'Precondition Failed', status_code=412)
        else:
            current = self.revisions.get(self.generation)
            if current is None:
                raise error_response.ErrorResponse(
                    'Object not found', status_code=404)
            metageneration = current.metadata.get('metageneration')
            if (metageneration_not_match is not None
                    and int(metageneration_not_match) == metageneration):
                raise error_response.ErrorResponse(
                    'Precondition Failed', status_code=412)
            if (metageneration_match is not None
                    and int(metageneration_match) != metageneration):
                raise error_response.ErrorResponse(
                    'Precondition Failed', status_code=412)

    def check_preconditions(
            self,
            request,
            if_generation_match='ifGenerationMatch',
            if_generation_not_match='ifGenerationNotMatch',
            if_metageneration_match='ifMetagenerationMatch',
            if_metageneration_not_match='ifMetagenerationNotMatch'):
        """Verify that the preconditions in request are met.

        :param request:flask.Request the http request.
        :param if_generation_match:str the name of the generation match
            parameter name, typically 'ifGenerationMatch', but sometimes
            'ifSourceGenerationMatch'.
        :param if_generation_not_match:str the name of the generation not-match
            parameter name, typically 'ifGenerationNotMatch', but sometimes
            'ifSourceGenerationNotMatch'.
        :param if_metageneration_match:str the name of the metageneration match
            parameter name, typically 'ifMetagenerationMatch', but sometimes
            'ifSourceMetagenerationMatch'.
        :param if_metageneration_not_match:str the name of the metageneration
            not-match parameter name, typically 'ifMetagenerationNotMatch', but
            sometimes 'ifSourceMetagenerationNotMatch'.
        :rtype:NoneType
        """
        generation_match = request.args.get(if_generation_match)
        generation_not_match = request.args.get(if_generation_not_match)
        metageneration_match = request.args.get(if_metageneration_match)
        metageneration_not_match = request.args.get(
            if_metageneration_not_match)
        self.check_preconditions_by_value(
            generation_match, generation_not_match, metageneration_match,
            metageneration_not_match)

    def _insert_revision(self, revision):
        """Insert a new revision that has been initialized and checked.

        :param revision:GcsObjectVersion the new revision to insert.
        :rtype:NoneType
        """
        update = {self.generation: revision}
        bucket = lookup_bucket(self.bucket_name)
        if not bucket.versioning_enabled():
            self.revisions = update
        else:
            self.revisions.update(update)

    def insert(self, gcs_url, request):
        """Insert a new revision based on the give flask request.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :return: the newly created object version.
        :rtype: GcsObjectVersion
        """
        media = testbench_utils.extract_media(request)
        self.generation += 1
        revision = GcsObjectVersion(gcs_url, self.bucket_name, self.name,
                                    self.generation, request, media)
        self._insert_revision(revision)
        return revision

    def _parse_part(self, multipart_upload_part):
        """Parse a portion of a multipart breaking out the headers and payload.

        :param multipart_upload_part:str a portion of the multipart upload body.
        :return: a tuple with the headers and the payload.
        :rtype: (dict, str)
        """
        headers = dict()
        index = 0
        next_line = multipart_upload_part.find('\r\n', index)
        while next_line != index:
            header_line = multipart_upload_part[index:next_line]
            key, value = header_line.split(': ', 2)
            # This does not work for repeated headers, but we do not expect
            # those in the testbench.
            headers[key.encode('ascii', 'ignore')] = value
            index = next_line + 2
            next_line = multipart_upload_part.find('\r\n', index)
        return headers, multipart_upload_part[next_line + 2:]

    def insert_multipart(self, gcs_url, request):
        """Insert a new revision based on the give flask request.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :return: the newly created object version.
        :rtype: GcsObjectVersion
        """
        content_type = request.headers.get('content-type')
        if content_type is None or not content_type.startswith(
                'multipart/related'):
            raise error_response.ErrorResponse(
                'Missing or invalid content-type header in multipart upload')
        _, _, boundary = content_type.partition('boundary=')
        if boundary is None:
            raise error_response.ErrorResponse(
                'Missing boundary (%s) in content-type header in multipart upload'
                % boundary)

        marker = '--' + boundary + '\r\n'
        body = testbench_utils.extract_media(request)
        parts = body.split(marker)
        # parts[0] is the empty string, `multipart` should start with the boundary
        # parts[1] is the JSON resource object part, with some headers
        resource_headers, resource_body = self._parse_part(parts[1])
        # parts[2] is the media, with some headers
        media_headers, media_body = self._parse_part(parts[2])
        end = media_body.find('\r\n--' + boundary + '--\r\n')
        if end == -1:
            raise error_response.ErrorResponse(
                'Missing end marker (--%s--) in media body' % boundary)
        media_body = media_body[:end]
        self.generation += 1
        revision = GcsObjectVersion(gcs_url, self.bucket_name, self.name,
                                    self.generation, request, media_body)
        # Apply any overrides from the resource object part.
        revision.update_from_metadata(json.loads(resource_body))
        # The content-type needs to be patched up, yuck.
        if media_headers.get('content-type') is not None:
            revision.update_from_metadata({
                'contentType':
                media_headers.get('content-type')
            })
        self._insert_revision(revision)
        return revision

    def insert_xml(self, gcs_url, request):
        """Implement the insert operation using the XML API.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :return: the newly created object version.
        :rtype: GcsObjectVersion
        """
        media = testbench_utils.extract_media(request)
        self.generation += 1
        goog_hash = request.headers.get('x-goog-hash')
        md5hash = None
        if goog_hash is not None:
            for hash in goog_hash.split(','):
                if hash.startswith('md5='):
                    md5hash = hash[4:]
        revision = GcsObjectVersion(gcs_url, self.bucket_name, self.name,
                                    self.generation, request, media)
        if md5hash is not None:
            revision.update_from_metadata({
                'md5Hash': md5hash,
            })
        self._insert_revision(revision)
        return revision

    def copy_from(self, gcs_url, request, source_revision):
        """Insert a new revision based on the give flask request.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :param source_revision:GcsObjectVersion the source object version to
            copy from.
        :return: the newly created object version.
        :rtype: GcsObjectVersion
        """
        self.generation += 1
        source_revision.validate_encryption_for_read(request)
        revision = GcsObjectVersion(gcs_url, self.bucket_name, self.name,
                                    self.generation, request,
                                    source_revision.media)
        revision.reset_predefined_acl(
            request.args.get('destinationPredefinedAcl'))
        metadata = json.loads(request.data)
        revision.update_from_metadata(metadata)
        self._insert_revision(revision)
        return revision

    def compose_from(self, gcs_url, request, composed_media):
        """Compose a new revision based on the give flask request.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :param composed_media:str contents of the composed object
        :return: the newly created object version.
        :rtype: GcsObjectVersion
        """
        self.generation += 1
        revision = GcsObjectVersion(gcs_url, self.bucket_name, self.name,
                                    self.generation, request, composed_media)
        revision.reset_predefined_acl(
            request.args.get('destinationPredefinedAcl'))
        payload = json.loads(request.data)
        if payload.get('destination') is not None:
            revision.update_from_metadata(payload.get('destination'))
        self._insert_revision(revision)
        return revision

    @classmethod
    def rewrite_fixed_args(cls):
        """The arguments that should not change between requests for the same
        rewrite operation."""
        return [
            'destinationKmsKeyName', 'destinationPredefinedAcl',
            'ifGenerationMatch', 'ifGenerationNotMatch',
            'ifMetagenerationMatch', 'ifMetagenerationNotMatch',
            'ifSourceGenerationMatch', 'ifSourceGenerationNotMatch',
            'ifSourceMetagenerationMatch', 'ifSourceMetagenerationNotMatch',
            'maxBytesRewrittenPerCall', 'projection', 'sourceGeneration',
            'userProject'
        ]

    @classmethod
    def capture_rewrite_operation_arguments(cls, request, destination_bucket,
                                            destination_object):
        """Captures the arguments used to validate related rewrite calls.

        :rtype:dict
        """
        original_arguments = {}
        for arg in GcsObject.rewrite_fixed_args():
            original_arguments[arg] = request.args.get(arg)
        original_arguments.update({
            'destination_bucket': destination_bucket,
            'destination_object': destination_object,
        })
        return original_arguments

    @classmethod
    def make_rewrite_token(cls, operation, destination_bucket,
                           destination_object, generation):
        """Create a new rewrite token for the given operation."""
        return base64.b64encode('/'.join([
            str(operation.get('id')),
            destination_bucket,
            destination_object,
            str(generation),
            str(operation.get('bytes_rewritten')),
        ]))

    def make_rewrite_operation(self, request, destination_bucket,
                               destination_object):
        """Create a new rewrite token for `Objects: rewrite`."""
        generation = request.args.get('sourceGeneration')
        if generation is None:
            generation = self.generation
        else:
            generation = int(generation)

        self.rewrite_token_generator = self.rewrite_token_generator + 1
        body = json.loads(request.data)
        original_arguments = self.capture_rewrite_operation_arguments(
            request, destination_object, destination_object)
        operation = {
            'id': self.rewrite_token_generator,
            'original_arguments': original_arguments,
            'actual_generation': generation,
            'bytes_rewritten': 0,
            'body': body,
        }
        token = GcsObject.make_rewrite_token(operation, destination_bucket,
                                             destination_object, generation)
        return token, operation

    def rewrite_finish(self, gcs_url, request, body, source):
        """Complete a rewrite from `source` into this object.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :param body:dict the HTTP payload, parsed via json.loads()
        :param source:GcsObjectVersion the source object version.
        :return: the newly created object version.
        :rtype:GcsObjectVersion
        """
        media = source.media
        self.check_preconditions(request)
        self.generation += 1
        revision = GcsObjectVersion(gcs_url, self.bucket_name, self.name,
                                    self.generation, request, media)
        revision.update_from_metadata(body)
        self._insert_revision(revision)
        return revision

    def rewrite_step(self, gcs_url, request, destination_bucket,
                     destination_object):
        """Execute an iteration of `Objects: rewrite.

        Objects: rewrite may need to be called multiple times before it
        succeeds. Only objects in the same location, with the same encryption,
        are guaranteed to complete in a single request.

        The implementation simulates some, but not all, the behaviors of the
        server, in particular, only rewrites within the same bucket and smaller
        than 1MiB complete immediately.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :param destination_bucket:str where will the object be placed after the
            rewrite operation completes.
        :param destination_object:str the name of the object when the rewrite
            operation completes.
        :return: a dictionary prepared for JSON encoding of a
            `Objects: rewrite` response.
        :rtype:dict
        """
        body = json.loads(request.data)
        rewrite_token = request.args.get('rewriteToken')
        if rewrite_token is not None and rewrite_token != '':
            # Note that we remove the rewrite operation, not just look it up.
            # That way if the operation completes in this call, and/or fails,
            # it is already removed. We need to insert it with a new token
            # anyway, so this makes sense.
            rewrite = self.rewrite_operations.pop(rewrite_token, None)
            if rewrite is None:
                raise error_response.ErrorResponse(
                    'Invalid or expired token in rewrite', status_code=410)
        else:
            rewrite_token, rewrite = self.make_rewrite_operation(
                request, destination_bucket, destination_bucket)

        # Compare the difference to the original arguments, on the first call
        # this is a waste, but the code is easier to follow.
        current_arguments = self.capture_rewrite_operation_arguments(
            request, destination_bucket, destination_object)
        diff = set(current_arguments) ^ set(rewrite.get('original_arguments'))
        if len(diff) != 0:
            raise error_response.ErrorResponse(
                'Mismatched arguments to rewrite', status_code=412)

        # This will raise if the version is deleted while the operation is in
        # progress.
        source = self.get_revision_by_generation(
            rewrite.get('actual_generation'))
        source.validate_encryption_for_read(
            request, prefix='x-goog-copy-source-encryption')
        bytes_rewritten = rewrite.get('bytes_rewritten')
        bytes_rewritten += (1024 * 1024)
        result = {
            'kind': 'storage#rewriteResponse',
            'objectSize': len(source.media),
        }
        if bytes_rewritten >= len(source.media):
            bytes_rewritten = len(source.media)
            rewrite['bytes_rewritten'] = bytes_rewritten
            # Success, the operation completed. Return the new object:
            object_path = destination_bucket + '/o/' + destination_object
            destination = GCS_OBJECTS.get(
                object_path, GcsObject(destination_bucket, destination_object))
            revision = destination.rewrite_finish(gcs_url, flask.request, body,
                                                  source)
            GCS_OBJECTS[object_path] = destination
            result['done'] = True
            result['resource'] = revision.metadata
            rewrite_token = ''
        else:
            rewrite['bytes_rewritten'] = bytes_rewritten
            rewrite_token = GcsObject.make_rewrite_token(
                rewrite, destination_bucket, destination_object,
                source.generation)
            self.rewrite_operations[rewrite_token] = rewrite
            result['done'] = False

        result.update({
            'totalBytesRewritten': bytes_rewritten,
            'rewriteToken': rewrite_token,
        })
        return result


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

    def update_from_metadata(self, metadata):
        """Update from a metadata dictionary.

        :param metadata:dict a dictionary with new metadata values.
        """
        tmp = self.metadata.copy()
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
        writeable_keys = {
            'acl', 'billing', 'cors', 'defaultObjectAcl', 'encryption',
            'labels', 'lifecycle', 'location', 'logging', 'storageClass',
            'versioning', 'website'
        }
        for key, value in patch.iteritems():
            if key not in writeable_keys:
                raise error_response.ErrorResponse(
                    'Invalid metadata change. %s is not writeable' % key,
                    status_code=503)
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


# Define the collection of GcsObjects indexed by <bucket_name>/o/<object_name>
GCS_OBJECTS = dict()

# Define the collection of Buckets indexed by <bucket_name>
GCS_BUCKETS = dict()

# Define the WSGI application to handle bucket requests.
GCS_HANDLER_PATH = '/storage/v1'
gcs = flask.Flask(__name__)
gcs.debug = True


# TODO(#821) TODO(#820) - until we implement CreateBucket + DeleteBucket insert
# a well-known bucket based on the BUCKET_NAME environment (this is set by our
# CI builds), to make the integration tests easy to write.
def insert_magic_bucket(base_url):
    if len(GCS_BUCKETS) == 0:
        bucket_name = os.environ.get('BUCKET_NAME', 'test-bucket')
        bucket = GcsBucket(base_url, bucket_name)
        # Enable versioning in the Bucket, the integration tests expect this to
        # be the case, this brings the metageneration number to 2.
        bucket.update_from_metadata({'versioning': {'enabled': True}})
        # Perform trivial updates that bring the metageneration to 4, the value
        # expected by the integration tests.
        bucket.update_from_metadata({})
        bucket.update_from_metadata({})
        GCS_BUCKETS[bucket_name] = bucket


def lookup_bucket(bucket_name):
    """Lookup a bucket by name in GCS_BUCKETS.

    :param bucket_name:str the name of the Bucket.
    :return: the bucket matching the name.
    :rtype:GcsBucket
    :raises:ErrorResponse if the bucket is not found.
    """
    bucket = GCS_BUCKETS.get(bucket_name)
    if bucket is None:
        raise error_response.ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    return bucket


def lookup_object(bucket_name, object_name):
    """Lookup an object by name in GCS_OBJECTS.

    :param bucket_name:str the name of the Bucket that contains the object.
    :param object_name:str the name of the Object.
    :return: tuple the object path and the object.
    :rtype: (str,GcsObject)
    :raises:ErrorResponse if the object is not found.
    """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path)
    if gcs_object is None:
        raise error_response.ErrorResponse(
            'Object %s in %s not found' % (object_name, bucket_name),
            status_code=404)
    return object_path, gcs_object


@gcs.route('/')
def gcs_index():
    """The default handler for GCS requests."""
    return 'OK'


@gcs.errorhandler(error_response.ErrorResponse)
def gcs_error(error):
    return error.as_response()


@gcs.route('/b')
def buckets_list():
    """Implement the 'Buckets: list' API: return the Buckets in a project."""
    base_url = flask.url_for('gcs_index', _external=True)
    project = flask.request.args.get('project')
    if project is None or project.endswith('-'):
        raise error_response.ErrorResponse(
            'Invalid or missing project id in `Buckets: list`')
    insert_magic_bucket(base_url)
    result = {'next_page_token': '', 'items': []}
    for name, b in GCS_BUCKETS.items():
        result['items'].append(b.metadata)
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b', methods=['POST'])
def buckets_insert():
    """Implement the 'Buckets: insert' API: create a new Bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    payload = json.loads(flask.request.data)
    bucket_name = payload.get('name')
    if bucket_name is None:
        raise error_response.ErrorResponse(
            'Missing bucket name in `Buckets: insert`', status_code=412)
    if not testbench_utils.validate_bucket_name(bucket_name):
        raise error_response.ErrorResponse(
            'Invalid bucket name in `Buckets: insert`')
    bucket = GCS_BUCKETS.get(bucket_name)
    if bucket is not None:
        raise error_response.ErrorResponse(
            'Bucket %s already exists' % bucket_name, status_code=503)
    bucket = GcsBucket(base_url, bucket_name)
    bucket.update_from_metadata(payload)
    GCS_BUCKETS[bucket_name] = bucket
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['PUT'])
def buckets_update(bucket_name):
    """Implement the 'Buckets: update' API: update an existing Bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    payload = json.loads(flask.request.data)
    name = payload.get('name')
    if name is None:
        raise error_response.ErrorResponse(
            'Missing bucket name in `Buckets: update`', status_code=412)
    if name != bucket_name:
        raise error_response.ErrorResponse(
            'Mismatched bucket name parameter in `Buckets: update`',
            status_code=400)
    bucket = lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    bucket.update_from_metadata(payload)
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>')
def buckets_get(bucket_name):
    """Implement the 'Buckets: get' API: return the metadata for a bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    bucket = lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['DELETE'])
def buckets_delete(bucket_name):
    """Implement the 'Buckets: delete' API."""
    bucket = lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    GCS_BUCKETS.pop(bucket_name, None)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>', methods=['PATCH'])
def buckets_patch(bucket_name):
    """Implement the 'Buckets: patch' API."""
    bucket = GCS_BUCKETS.get(bucket_name, None)
    if bucket is None:
        raise error_response.ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    bucket.check_preconditions(flask.request)
    patch = json.loads(flask.request.data)
    bucket.apply_patch(patch)
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>/acl')
def bucket_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    result = {
        'items': gcs_bucket.metadata.get('acl', []),
    }
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/acl', methods=['POST'])
def bucket_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    return testbench_utils.filtered_response(
        flask.request,
        gcs_bucket.insert_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['DELETE'])
def bucket_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    gcs_bucket.delete_acl(entity)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/acl/<entity>')
def bucket_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    acl = gcs_bucket.get_acl(entity)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PUT'])
def bucket_acl_update(bucket_name, entity):
    """Implement the 'BucketAccessControls: update' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PATCH'])
def bucket_acl_patch(bucket_name, entity):
    """Implement the 'BucketAccessControls: patch' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl')
def bucket_default_object_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    result = {
        'items': gcs_bucket.metadata.get('defaultObjectAcl', []),
    }
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/defaultObjectAcl', methods=['POST'])
def bucket_default_object_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    return testbench_utils.filtered_response(
        flask.request,
        gcs_bucket.insert_default_object_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['DELETE'])
def bucket_default_object_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    gcs_bucket.delete_default_object_acl(entity)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>')
def bucket_default_object_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    acl = gcs_bucket.get_default_object_acl(entity)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PUT'])
def bucket_default_object_acl_update(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: update' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_default_object_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PATCH'])
def bucket_default_object_acl_patch(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: patch' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_default_object_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/notificationConfigs')
def bucket_notification_list(bucket_name):
    """Implement the 'Notifications: list' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request, {
        'kind': 'storage#notifications',
        'items': gcs_bucket.list_notifications()
    })


@gcs.route('/b/<bucket_name>/notificationConfigs', methods=['POST'])
def bucket_notification_create(bucket_name):
    """Implement the 'Notifications: insert' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    notification = gcs_bucket.insert_notification(flask.request)
    return testbench_utils.filtered_response(flask.request, notification)


@gcs.route(
    '/b/<bucket_name>/notificationConfigs/<notification_id>',
    methods=['DELETE'])
def bucket_notification_delete(bucket_name, notification_id):
    """Implement the 'Notifications: delete' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    gcs_bucket.delete_notification(notification_id)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/notificationConfigs/<notification_id>')
def bucket_notification_get(bucket_name, notification_id):
    """Implement the 'Notifications: get' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    notification = gcs_bucket.get_notification(notification_id)
    return testbench_utils.filtered_response(flask.request, notification)


@gcs.route('/b/<bucket_name>/iam')
def bucket_get_iam_policy(bucket_name):
    """Implement the 'Buckets: getIamPolicy' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request,
                             gcs_bucket.get_iam_policy(flask.request))


@gcs.route('/b/<bucket_name>/iam', methods=['PUT'])
def bucket_set_iam_policy(bucket_name):
    """Implement the 'Buckets: setIamPolicy' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request,
                             gcs_bucket.set_iam_policy(flask.request))


@gcs.route('/b/<bucket_name>/iam/testPermissions')
def bucket_test_iam_permissions(bucket_name):
    """Implement the 'Buckets: testIamPermissions' API."""
    gcs_bucket = lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request,
                             gcs_bucket.test_iam_permissions(flask.request))


@gcs.route('/b/<bucket_name>/o')
def objects_list(bucket_name):
    """Implement the 'Objects: list' API: return the objects in a bucket."""
    # Lookup the bucket, if this fails the bucket does not exist, and this
    # function should return an error.
    _ = lookup_bucket(bucket_name)
    result = {'next_page_token': '', 'items': []}
    versions_parameter = flask.request.args.get('versions')
    all_versions = (versions_parameter is not None
                    and bool(versions_parameter))
    for name, o in GCS_OBJECTS.items():
        if name.find(bucket_name + '/o') != 0:
            continue
        if o.get_latest() is None:
            continue
        if all_versions:
            for object_version in o.revisions.itervalues():
                result['items'].append(object_version.metadata)
        else:
            result['items'].append(o.get_latest().metadata)
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route(
    '/b/<source_bucket>/o/<source_object>/copyTo/b/<destination_bucket>/o/<destination_object>',
    methods=['POST'])
def objects_copy(source_bucket, source_object, destination_bucket,
                 destination_object):
    """Implement the 'Objects: copy' API, copy an object."""
    object_path, gcs_object = lookup_object(source_bucket, source_object)
    gcs_object.check_preconditions(
        flask.request,
        if_generation_match='ifSourceGenerationMatch',
        if_generation_not_match='ifSourceGenerationNotMatch',
        if_metageneration_match='ifSourceMetagenerationMatch',
        if_metageneration_not_match='ifSourceMetagenerationNotMatch')
    source_revision = gcs_object.get_revision(flask.request,
                                              'sourceGeneration')
    if source_revision is None:
        raise error_response.ErrorResponse(
            'Revision not found %s' % object_path, status_code=404)

    destination_path = destination_bucket + "/o/" + destination_object
    gcs_object = GCS_OBJECTS.setdefault(
        destination_path, GcsObject(destination_bucket, destination_object))
    base_url = flask.url_for('gcs_index', _external=True)
    current_version = gcs_object.copy_from(base_url, flask.request,
                                           source_revision)
    return testbench_utils.filtered_response(flask.request, current_version.metadata)


@gcs.route(
    '/b/<source_bucket>/o/<source_object>/rewriteTo/b/<destination_bucket>/o/<destination_object>',
    methods=['POST'])
def objects_rewrite(source_bucket, source_object, destination_bucket,
                    destination_object):
    """Implement the 'Objects: rewrite' API."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    object_path, gcs_object = lookup_object(source_bucket, source_object)
    gcs_object.check_preconditions(
        flask.request,
        if_generation_match='ifSourceGenerationMatch',
        if_generation_not_match='ifSourceGenerationNotMatch',
        if_metageneration_match='ifSourceMetagenerationMatch',
        if_metageneration_not_match='ifSourceMetagenerationNotMatch')
    response = gcs_object.rewrite_step(base_url, flask.request,
                                       destination_bucket, destination_object)
    return testbench_utils.filtered_response(flask.request, response)


@gcs.route('/b/<bucket_name>/o/<object_name>')
def objects_get(bucket_name, object_name):
    """Implement the 'Objects: get' API.  Read objects or their metadata."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)

    media = flask.request.args.get('alt', None)
    if media is None or media == 'json':
        return testbench_utils.filtered_response(flask.request, revision.metadata)
    if media != 'media':
        raise error_response.ErrorResponse('Invalid alt=%s parameter' % media)
    revision.validate_encryption_for_read(flask.request)
    response = flask.make_response(revision.media)
    length = len(revision.media)
    response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1, length)
    return response


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['DELETE'])
def objects_delete(bucket_name, object_name):
    """Implement the 'Objects: delete' API.  Delete objects."""
    object_path, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    remove = gcs_object.del_revision(flask.request)
    if remove:
        GCS_OBJECTS.pop(object_path)

    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['PUT'])
def objects_update(bucket_name, object_name):
    """Implement the 'Objects: update' API: update an existing Object."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.update_revision(flask.request)
    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>/compose', methods=['POST'])
def objects_compose(bucket_name, object_name):
    """Implement the 'Objects: compose' API: concatenate Objects."""
    composed_object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(composed_object_path,
                                 GcsObject(bucket_name, object_name))
    if gcs_object is not None:
        gcs_object.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    source_objects = payload["sourceObjects"]
    if source_objects is None:
        raise error_response.ErrorResponse(
            'You must provide at least one source component.', status_code=400)
    if len(source_objects) > 32:
        raise error_response.ErrorResponse(
            'The number of source components provided'
            ' (%d) exceeds the maximum (32)' % len(source_objects),
            status_code=400)
    composed_media = ""
    for source_object in source_objects:
        source_object_name = source_object.get('name')
        if source_object_name is None:
            raise error_response.ErrorResponse('Required.', status_code=400)
        source_object_path, source_gcs_object = lookup_object(
            bucket_name, source_object_name)
        source_revision = source_gcs_object.get_latest()
        generation = source_object.get('generation')
        if generation is not None:
            source_revision = source_gcs_object.get_revision_by_generation(
                generation)
            if source_revision is None:
                raise error_response.ErrorResponse(
                    'No such object: %s' % source_object_path, status_code=404)
        object_preconditions = source_object.get('objectPreconditions')
        if object_preconditions is not None:
            if_generation_match = object_preconditions.get('ifGenerationMatch')
            source_gcs_object.check_preconditions_by_value(
                if_generation_match, None, None, None)
        composed_media += source_revision.media
    gcs_object = GCS_OBJECTS.setdefault(composed_object_path,
                                        GcsObject(bucket_name, object_name))
    GCS_OBJECTS[composed_object_path] = gcs_object
    base_url = flask.url_for('gcs_index', _external=True)
    current_version = gcs_object.compose_from(base_url, flask.request,
                                              composed_media)
    return testbench_utils.filtered_response(flask.request, current_version.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['PATCH'])
def objects_patch(bucket_name, object_name):
    """Implement the 'Objects: patch' API: update an existing Object."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.patch_revision(flask.request)
    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl')
def objects_acl_list(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: list' API."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    result = {
        'items': revision.metadata.get('acl', []),
    }
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl', methods=['POST'])
def objects_acl_create(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: create' API."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    return testbench_utils.filtered_response(
        flask.request,
        revision.insert_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['DELETE'])
def objects_acl_delete(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: delete' API."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    revision.delete_acl(entity)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>')
def objects_acl_get(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: get' API."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    acl = revision.get_acl(entity)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PUT'])
def objects_acl_update(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: update' API."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    acl = revision.update_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PATCH'])
def objects_acl_patch(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: patch' API."""
    _, gcs_object = lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    acl = revision.patch_acl(entity, flask.request)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/projects/<project_id>/serviceAccount')
def projects_get(project_id):
    """Implement the `Projects.serviceAccount: get` API."""
    return testbench_utils.filtered_response(
        flask.request, {
            'kind':
            'storage#serviceAccount',
            'email_address':
            'service-123456789@gs-project-accounts.iam.gserviceaccount.com'
        })


# Define the WSGI application to handle bucket requests.
UPLOAD_HANDLER_PATH = '/upload/storage/v1'
upload = flask.Flask(__name__)
upload.debug = True


@upload.errorhandler(error_response.ErrorResponse)
def upload_error(error):
    return error.as_response()


@upload.route('/b/<bucket_name>/o', methods=['POST'])
def objects_insert(bucket_name):
    """Implement the 'Objects: insert' API.  Insert a new GCS Object."""
    gcs_url = flask.url_for(
        'objects_insert', bucket_name=bucket_name, _external=True).replace(
            '/upload/', '/')
    insert_magic_bucket(gcs_url)
    object_name = flask.request.args.get('name', None)
    if object_name is None:
        raise error_response.ErrorResponse(
            'name not set in Objects: insert', status_code=412)
    upload_type = flask.request.args.get('uploadType')
    if upload_type is None:
        raise error_response.ErrorResponse(
            'uploadType not set in Objects: insert', status_code=412)
    if upload_type not in {'multipart', 'media'}:
        raise error_response.ErrorResponse(
            'testbench does not support %s uploadType' % upload_type,
            status_code=400)
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    GCS_OBJECTS[object_path] = gcs_object
    if upload_type == 'media':
        current_version = gcs_object.insert(gcs_url, flask.request)
    else:
        current_version = gcs_object.insert_multipart(gcs_url, flask.request)
    return testbench_utils.filtered_response(flask.request, current_version.metadata)


# Define the WSGI application to handle (a few) requests in the XML API.
XMLAPI_HANDLER_PATH = '/xmlapi'
xmlapi = flask.Flask(__name__)
xmlapi.debug = True


@xmlapi.errorhandler(error_response.ErrorResponse)
def xmlapi_error(error):
    return error.as_response()


@xmlapi.route('/<bucket_name>/<object_name>')
def xmlapi_get_object(bucket_name, object_name):
    """Implement the 'Objects: insert' API.  Insert a new GCS Object."""
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path)
    if gcs_object is None:
        raise error_response.ErrorResponse(
            'Object not found %s' % object_path, status_code=404)
    if flask.request.args.get('acl') is not None:
        raise error_response.ErrorResponse(
            'ACL query not supported in XML API', status_code=500)
    if flask.request.args.get('encryption') is not None:
        raise error_response.ErrorResponse(
            'Encryption query not supported in XML API', status_code=500)
    generation_match = flask.request.headers.get('if-generation-match')
    metageneration_match = flask.request.headers.get('if-metageneration-match')
    gcs_object.check_preconditions_by_value(generation_match, None,
                                            metageneration_match, None)
    revision = gcs_object.get_revision(flask.request)
    response = flask.make_response(revision.media)
    length = len(revision.media)
    response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1, length)
    return response


@xmlapi.route('/<bucket_name>/<object_name>', methods=['PUT'])
def xmlapi_put_object(bucket_name, object_name):
    """Inserts a new GCS Object.

    Implement the PUT request in the XML API.
    """
    gcs_url = flask.url_for(
        'xmlapi_put_object',
        bucket_name=bucket_name,
        object_name=object_name,
        _external=True).replace('/xmlapi/', '/')
    insert_magic_bucket(gcs_url)
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    generation_match = flask.request.headers.get('x-goog-if-generation-match')
    metageneration_match = flask.request.headers.get('x-goog-if-metageneration-match')
    gcs_object.check_preconditions_by_value(generation_match, None,
                                            metageneration_match, None)
    GCS_OBJECTS[object_path] = gcs_object
    gcs_object.insert_xml(gcs_url, flask.request)
    return ''


application = wsgi.DispatcherMiddleware(
    root, {
        '/httpbin': httpbin.app,
        GCS_HANDLER_PATH: gcs,
        UPLOAD_HANDLER_PATH: upload,
        XMLAPI_HANDLER_PATH: xmlapi,
    })


def main():
    """Parse the arguments and run the test bench application."""
    parser = argparse.ArgumentParser(
        description='A testbench for the Google Cloud C++ Client Library')
    parser.add_argument(
        '--host', default='localhost', help='The listening port')
    parser.add_argument('--port', help='The listening port')
    # By default we do not turn on the debugging. This typically runs inside a
    # Docker image, with a uid that has not entry in /etc/passwd, and the
    # werkzeug debugger crashes in that environment (as it should probably).
    parser.add_argument(
        '--debug',
        help='Use the WSGI debugger',
        default=False,
        action='store_true')
    arguments = parser.parse_args()

    # Compose the different WSGI applications.
    serving.run_simple(
        arguments.host,
        int(arguments.port),
        application,
        use_reloader=True,
        use_debugger=arguments.debug,
        use_evalex=True)


if __name__ == '__main__':
    main()
