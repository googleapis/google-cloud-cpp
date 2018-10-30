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
import gcs_bucket
import gcs_object
import httpbin
import json
import os
import testbench_utils
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


# Define the WSGI application to handle bucket requests.
GCS_HANDLER_PATH = '/storage/v1'
gcs = flask.Flask(__name__)
gcs.debug = True


def insert_magic_bucket(base_url):
    if len(testbench_utils.all_buckets()) == 0:
        bucket_name = os.environ.get('BUCKET_NAME', 'test-bucket')
        bucket = gcs_bucket.GcsBucket(base_url, bucket_name)
        # Enable versioning in the Bucket, the integration tests expect this to
        # be the case, this brings the metageneration number to 2.
        bucket.update_from_metadata({'versioning': {'enabled': True}})
        # Perform trivial updates that bring the metageneration to 4, the value
        # expected by the integration tests.
        bucket.update_from_metadata({})
        bucket.update_from_metadata({})
        testbench_utils.insert_bucket(bucket_name, bucket)


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
    for name, b in testbench_utils.all_buckets():
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
    if testbench_utils.has_bucket(bucket_name):
        raise error_response.ErrorResponse(
            'Bucket %s already exists' % bucket_name, status_code=400)
    bucket = gcs_bucket.GcsBucket(base_url, bucket_name)
    bucket.update_from_metadata(payload)
    testbench_utils.insert_bucket(bucket_name, bucket)
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
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    bucket.update_from_metadata(payload)
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>')
def buckets_get(bucket_name):
    """Implement the 'Buckets: get' API: return the metadata for a bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['DELETE'])
def buckets_delete(bucket_name):
    """Implement the 'Buckets: delete' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    testbench_utils.delete_bucket(bucket_name)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>', methods=['PATCH'])
def buckets_patch(bucket_name):
    """Implement the 'Buckets: patch' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    patch = json.loads(flask.request.data)
    bucket.apply_patch(patch)
    return testbench_utils.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>/acl')
def bucket_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    result = {
        'items': bucket.metadata.get('acl', []),
    }
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/acl', methods=['POST'])
def bucket_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    return testbench_utils.filtered_response(
        flask.request,
        bucket.insert_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['DELETE'])
def bucket_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    bucket.delete_acl(entity)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/acl/<entity>')
def bucket_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    acl = bucket.get_acl(entity)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PUT'])
def bucket_acl_update(bucket_name, entity):
    """Implement the 'BucketAccessControls: update' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = bucket.update_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PATCH'])
def bucket_acl_patch(bucket_name, entity):
    """Implement the 'BucketAccessControls: patch' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = bucket.update_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl')
def bucket_default_object_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    result = {
        'items': bucket.metadata.get('defaultObjectAcl', []),
    }
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/defaultObjectAcl', methods=['POST'])
def bucket_default_object_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    return testbench_utils.filtered_response(
        flask.request,
        bucket.insert_default_object_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['DELETE'])
def bucket_default_object_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    bucket.delete_default_object_acl(entity)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>')
def bucket_default_object_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    acl = bucket.get_default_object_acl(entity)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PUT'])
def bucket_default_object_acl_update(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: update' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = bucket.update_default_object_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PATCH'])
def bucket_default_object_acl_patch(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: patch' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = bucket.update_default_object_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/notificationConfigs')
def bucket_notification_list(bucket_name):
    """Implement the 'Notifications: list' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(flask.request, {
        'kind': 'storage#notifications',
        'items': bucket.list_notifications()
    })


@gcs.route('/b/<bucket_name>/notificationConfigs', methods=['POST'])
def bucket_notification_create(bucket_name):
    """Implement the 'Notifications: insert' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    notification = bucket.insert_notification(flask.request)
    return testbench_utils.filtered_response(flask.request, notification)


@gcs.route(
    '/b/<bucket_name>/notificationConfigs/<notification_id>',
    methods=['DELETE'])
def bucket_notification_delete(bucket_name, notification_id):
    """Implement the 'Notifications: delete' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    bucket.delete_notification(notification_id)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/notificationConfigs/<notification_id>')
def bucket_notification_get(bucket_name, notification_id):
    """Implement the 'Notifications: get' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    notification = bucket.get_notification(notification_id)
    return testbench_utils.filtered_response(flask.request, notification)


@gcs.route('/b/<bucket_name>/iam')
def bucket_get_iam_policy(bucket_name):
    """Implement the 'Buckets: getIamPolicy' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(
        flask.request, bucket.get_iam_policy(flask.request))


@gcs.route('/b/<bucket_name>/iam', methods=['PUT'])
def bucket_set_iam_policy(bucket_name):
    """Implement the 'Buckets: setIamPolicy' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(
        flask.request, bucket.set_iam_policy(flask.request))


@gcs.route('/b/<bucket_name>/iam/testPermissions')
def bucket_test_iam_permissions(bucket_name):
    """Implement the 'Buckets: testIamPermissions' API."""
    bucket = testbench_utils.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    return testbench_utils.filtered_response(
        flask.request, bucket.test_iam_permissions(flask.request))


@gcs.route('/b/<bucket_name>/o')
def objects_list(bucket_name):
    """Implement the 'Objects: list' API: return the objects in a bucket."""
    # Lookup the bucket, if this fails the bucket does not exist, and this
    # function should return an error.
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    _ = testbench_utils.lookup_bucket(bucket_name)
    result = {'next_page_token': '', 'items': []}
    versions_parameter = flask.request.args.get('versions')
    all_versions = (versions_parameter is not None
                    and bool(versions_parameter))
    for name, o in testbench_utils.all_objects():
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
    object_path, blob = testbench_utils.lookup_object(source_bucket, source_object)
    blob.check_preconditions(
        flask.request,
        if_generation_match='ifSourceGenerationMatch',
        if_generation_not_match='ifSourceGenerationNotMatch',
        if_metageneration_match='ifSourceMetagenerationMatch',
        if_metageneration_not_match='ifSourceMetagenerationNotMatch')
    source_revision = blob.get_revision(flask.request,
                                        'sourceGeneration')
    if source_revision is None:
        raise error_response.ErrorResponse(
            'Revision not found %s' % object_path, status_code=404)

    destination_path, destination = testbench_utils.get_object(
        destination_bucket, destination_object,
        gcs_object.GcsObject(destination_bucket, destination_object))
    base_url = flask.url_for('gcs_index', _external=True)
    current_version = destination.copy_from(base_url, flask.request,
                                            source_revision)
    testbench_utils.insert_object(destination_path, destination)
    return testbench_utils.filtered_response(flask.request, current_version.metadata)


@gcs.route(
    '/b/<source_bucket>/o/<source_object>/rewriteTo/b/<destination_bucket>/o/<destination_object>',
    methods=['POST'])
def objects_rewrite(source_bucket, source_object, destination_bucket,
                    destination_object):
    """Implement the 'Objects: rewrite' API."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    object_path, blob = testbench_utils.lookup_object(source_bucket, source_object)
    blob.check_preconditions(
        flask.request,
        if_generation_match='ifSourceGenerationMatch',
        if_generation_not_match='ifSourceGenerationNotMatch',
        if_metageneration_match='ifSourceMetagenerationMatch',
        if_metageneration_not_match='ifSourceMetagenerationNotMatch')
    response = blob.rewrite_step(base_url, flask.request,
                                 destination_bucket, destination_object)
    return testbench_utils.filtered_response(flask.request, response)


@gcs.route('/b/<bucket_name>/o/<object_name>')
def objects_get(bucket_name, object_name):
    """Implement the 'Objects: get' API.  Read objects or their metadata."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)

    media = flask.request.args.get('alt', None)
    if media is None or media == 'json':
        return testbench_utils.filtered_response(flask.request, revision.metadata)
    if media != 'media':
        raise error_response.ErrorResponse('Invalid alt=%s parameter' % media)
    revision.validate_encryption_for_read(flask.request)
    instructions = flask.request.headers.get('x-goog-testbench-instructions')
    if instructions == 'return-corrupted-data':
        response_payload = testbench_utils.corrupt_media(revision.media)
    else:
        response_payload = revision.media
    response = flask.make_response(response_payload)
    length = len(response_payload)
    response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1, length)
    response.headers['x-goog-hash'] = revision.x_goog_hash_header()
    return response


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['DELETE'])
def objects_delete(bucket_name, object_name):
    """Implement the 'Objects: delete' API.  Delete objects."""
    object_path, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    remove = blob.del_revision(flask.request)
    if remove:
        testbench_utils.delete_object(object_path)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['PUT'])
def objects_update(bucket_name, object_name):
    """Implement the 'Objects: update' API: update an existing Object."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.update_revision(flask.request)
    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>/compose', methods=['POST'])
def objects_compose(bucket_name, object_name):
    """Implement the 'Objects: compose' API: concatenate Objects."""
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
        source_object_path, source_blob = testbench_utils.lookup_object(
            bucket_name, source_object_name)
        source_revision = source_blob.get_latest()
        generation = source_object.get('generation')
        if generation is not None:
            source_revision = source_blob.get_revision_by_generation(
                generation)
            if source_revision is None:
                raise error_response.ErrorResponse(
                    'No such object: %s' % source_object_path, status_code=404)
        object_preconditions = source_object.get('objectPreconditions')
        if object_preconditions is not None:
            if_generation_match = object_preconditions.get('ifGenerationMatch')
            source_blob.check_preconditions_by_value(
                if_generation_match, None, None, None)
        composed_media += source_revision.media
    composed_object_path, composed_object = testbench_utils.get_object(
        bucket_name, object_name, gcs_object.GcsObject(bucket_name, object_name))
    composed_object.check_preconditions(flask.request)
    base_url = flask.url_for('gcs_index', _external=True)
    current_version = composed_object.compose_from(base_url, flask.request,
                                                   composed_media)
    testbench_utils.insert_object(composed_object_path, composed_object)
    return testbench_utils.filtered_response(flask.request, current_version.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['PATCH'])
def objects_patch(bucket_name, object_name):
    """Implement the 'Objects: patch' API: update an existing Object."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.patch_revision(flask.request)
    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl')
def objects_acl_list(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: list' API."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)
    result = {
        'items': revision.metadata.get('acl', []),
    }
    return testbench_utils.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl', methods=['POST'])
def objects_acl_create(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: create' API."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    return testbench_utils.filtered_response(
        flask.request,
        revision.insert_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['DELETE'])
def objects_acl_delete(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: delete' API."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)
    revision.delete_acl(entity)
    return testbench_utils.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>')
def objects_acl_get(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: get' API."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)
    acl = revision.get_acl(entity)
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PUT'])
def objects_acl_update(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: update' API."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    acl = revision.update_acl(entity, payload.get('role', ''))
    return testbench_utils.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PATCH'])
def objects_acl_patch(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: patch' API."""
    _, blob = testbench_utils.lookup_object(bucket_name, object_name)
    blob.check_preconditions(flask.request)
    revision = blob.get_revision(flask.request)
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
    object_path, blob = testbench_utils.get_object(
        bucket_name, object_name, gcs_object.GcsObject(bucket_name, object_name))
    blob.check_preconditions(flask.request)
    if upload_type == 'media':
        current_version = blob.insert(gcs_url, flask.request)
    else:
        current_version = blob.insert_multipart(gcs_url, flask.request)
    testbench_utils.insert_object(object_path, blob)
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
    object_path, blob = testbench_utils.lookup_object(bucket_name, object_name)
    if flask.request.args.get('acl') is not None:
        raise error_response.ErrorResponse(
            'ACL query not supported in XML API', status_code=500)
    if flask.request.args.get('encryption') is not None:
        raise error_response.ErrorResponse(
            'Encryption query not supported in XML API', status_code=500)
    generation_match = flask.request.headers.get('if-generation-match')
    metageneration_match = flask.request.headers.get('if-metageneration-match')
    blob.check_preconditions_by_value(generation_match, None,
                                      metageneration_match, None)
    revision = blob.get_revision(flask.request)
    instructions = flask.request.headers.get('x-goog-testbench-instructions')
    if instructions == 'return-corrupted-data':
        response_payload = testbench_utils.corrupt_media(revision.media)
    else:
        response_payload = revision.media
    response = flask.make_response(response_payload)
    length = len(response_payload)
    response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1, length)
    response.headers['x-goog-hash'] = revision.x_goog_hash_header()
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
    object_path, blob = testbench_utils.get_object(
        bucket_name, object_name, gcs_object.GcsObject(bucket_name, object_name))
    generation_match = flask.request.headers.get('x-goog-if-generation-match')
    metageneration_match = flask.request.headers.get('x-goog-if-metageneration-match')
    blob.check_preconditions_by_value(generation_match, None,
                                      metageneration_match, None)
    revision = blob.insert_xml(gcs_url, flask.request)
    testbench_utils.insert_object(object_path, blob)
    response = flask.make_response('')
    response.headers['x-goog-hash'] = revision.x_goog_hash_header()
    return response


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
