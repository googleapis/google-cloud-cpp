#!/usr/bin/env python
# Copyright 2018 Google LLC.
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
"""A class to simulate a GCS object in the client test bench."""

import base64
import flask
import json
import gcs_object_version
import gcs_service


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
        :rtype: gcs_object_version.GcsObjectVersion
        :raises:ErrorResponse if the request contains an invalid generation
            number.
        """
        generation = request.args.get(version_field_name)
        if generation is None:
            return self.get_latest()
        version = self.revisions.get(int(generation))
        if version is None:
            raise gcs_service.ErrorResponse(
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
                raise gcs_service.ErrorResponse(
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
                raise gcs_service.ErrorResponse(
                    'Precondition Failed: generation %s not found' %
                    generation)
        patch = json.loads(request.data)
        writeable_keys = {
            'acl', 'cacheControl', 'contentDisposition', 'contentEncoding',
            'contentLanguage', 'contentType', 'metadata'
        }
        for key, value in patch.iteritems():
            if key not in writeable_keys:
                raise gcs_service.ErrorResponse(
                    'Invalid metadata change. %s is not writeable' % key,
                    status_code=503)
        patched = gcs_service.json_api_patch(
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
            raise gcs_service.ErrorResponse(
                'Precondition Failed', status_code=412)
        # This object does not exist (yet), testing in this case is special.
        if (generation_not_match is not None
                and int(generation_not_match) == self.generation):
            raise gcs_service.ErrorResponse(
                'Precondition Failed', status_code=412)

        if self.generation == 0:
            if (metageneration_match is not None
                    or metageneration_not_match is not None):
                raise gcs_service.ErrorResponse(
                    'Precondition Failed', status_code=412)
        else:
            current = self.revisions.get(self.generation)
            if current is None:
                raise gcs_service.ErrorResponse(
                    'Object not found', status_code=404)
            metageneration = current.metadata.get('metageneration')
            if (metageneration_not_match is not None
                    and int(metageneration_not_match) == metageneration):
                raise gcs_service.ErrorResponse(
                    'Precondition Failed', status_code=412)
            if (metageneration_match is not None
                    and int(metageneration_match) != metageneration):
                raise gcs_service.ErrorResponse(
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
        bucket = gcs_service.lookup_bucket(self.bucket_name)
        if not bucket.versioning_enabled():
            self.revisions = update
        else:
            self.revisions.update(update)

    def insert(self, gcs_url, request):
        """Insert a new revision based on the give flask request.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :return: the newly created object version.
        :rtype: gcs_object_version.GcsObjectVersion
        """
        media = gcs_service.extract_media(request)
        self.generation += 1
        revision = gcs_object_version.GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request,
            media)
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
            key, value = header_line.split(':', 2)
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
        :rtype: gcs_object_version.GcsObjectVersion
        """
        content_type = request.headers.get('content-type')
        if content_type is None or not content_type.startswith(
                'multipart/related'):
            raise gcs_service.ErrorResponse(
                'Missing or invalid content-type header in multipart upload')
        _, _, boundary = content_type.partition('boundary=')
        if boundary is None:
            raise gcs_service.ErrorResponse(
                'Missing boundary (%s) in content-type header in multipart upload'
                % boundary)

        marker = '--' + boundary + '\r\n'
        body = gcs_service.extract_media(request)
        parts = body.split(marker)
        # parts[0] is the empty string, `multipart` should start with the boundary
        # parts[1] is the JSON resource object part, with some headers
        resource_headers, resource_body = self._parse_part(parts[1])
        # parts[2] is the media, with some headers
        media_headers, media_body = self._parse_part(parts[2])
        end = media_body.find('\r\n--' + boundary + '--\r\n')
        if end == -1:
            raise gcs_service.ErrorResponse(
                'Missing end marker (--%s--) in media body' % boundary)
        media_body = media_body[:end]
        self.generation += 1
        revision = gcs_object_version.GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request,
            media_body)
        # Apply any overrides from the resource object part.
        revision.update_from_metadata(json.loads(resource_body))
        # The content-type needs to be patched up, yuck.
        if resource_headers.get('content-type') is not None:
            revision.update_from_metadata({
                'contentType':
                resource_headers.get('content-type')
            })
        self._insert_revision(revision)
        return revision

    def insert_xml(self, gcs_url, request):
        """Implement the insert operation using the XML API.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :return: the newly created object version.
        :rtype: gcs_object_version.GcsObjectVersion
        """
        media = gcs_service.extract_media(request)
        goog_hash = request.headers.get('x-goog-hash')
        md5hash = None
        if goog_hash is not None:
            for hash in goog_hash.split(','):
                if hash.startswith('md5='):
                    md5hash = hash[4:]
        revision = gcs_object_version.GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request,
            media)
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
        :rtype: gcs_object_version.GcsObjectVersion
        """
        self.generation += 1
        source_revision.validate_encryption_for_read(request)
        revision = gcs_object_version.GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request,
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
        :rtype: gcs_object_version.GcsObjectVersion
        """
        self.generation += 1
        revision = gcs_object_version.GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request,
            composed_media)
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
        revision = gcs_object_version.GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request,
            media)
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
                raise gcs_service.ErrorResponse(
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
            raise gcs_service.ErrorResponse(
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
            _, destination = gcs_service.get_object_default(
                destination_bucket, destination_object,
                GcsObject(destination_bucket, destination_object))
            revision = destination.rewrite_finish(gcs_url, flask.request, body,
                                                  source)
            gcs_service.insert_object(destination_bucket, destination_object,
                                      destination)
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
