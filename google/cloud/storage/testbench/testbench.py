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
import json
import flask
import httpbin
import gcs_service
from werkzeug import serving
from werkzeug import wsgi


@httpbin.app.errorhandler(gcs_service.ErrorResponse)
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


@gcs.route('/')
def gcs_index():
    """The default handler for GCS requests."""
    return 'OK'


@gcs.errorhandler(gcs_service.ErrorResponse)
def gcs_error(error):
    return error.as_response()


@gcs.route('/b')
def buckets_list():
    """Implement the 'Buckets: list' API: return the Buckets in a project."""
    base_url = flask.url_for('gcs_index', _external=True)
    gcs_service.GcsBucket.insert_magic_bucket(base_url)
    result = {'next_page_token': '', 'items': []}
    for name, b in gcs_service.bucket_items():
        result['items'].append(b.metadata)
    return gcs_service.filtered_response(flask.request, result)


@gcs.route('/b', methods=['POST'])
def buckets_insert():
    """Implement the 'Buckets: insert' API: create a new Bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    gcs_service.GcsBucket.insert_magic_bucket(base_url)
    payload = json.loads(flask.request.data)
    bucket_name = payload.get('name')
    if bucket_name is None:
        raise gcs_service.ErrorResponse(
            'Missing bucket name in `Buckets: insert`', status_code=412)
    if gcs_service.has_bucket(bucket_name):
        raise gcs_service.ErrorResponse(
            'Bucket %s already exists' % bucket_name, status_code=503)
    bucket = gcs_service.GcsBucket(base_url, bucket_name)
    bucket.update_from_metadata(payload)
    gcs_service.insert_bucket(bucket_name, bucket)
    return gcs_service.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['PUT'])
def buckets_update(bucket_name):
    """Implement the 'Buckets: update' API: update an existing Bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    gcs_service.GcsBucket.insert_magic_bucket(base_url)
    payload = json.loads(flask.request.data)
    name = payload.get('name')
    if name is None:
        raise gcs_service.ErrorResponse(
            'Missing bucket name in `Buckets: update`', status_code=412)
    if name != bucket_name:
        raise gcs_service.ErrorResponse(
            'Mismatched bucket name parameter in `Buckets: update`',
            status_code=400)
    bucket = gcs_service.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    bucket.update_from_metadata(payload)
    return gcs_service.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>')
def buckets_get(bucket_name):
    """Implement the 'Buckets: get' API: return the metadata for a bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    gcs_service.GcsBucket.insert_magic_bucket(base_url)
    # TODO(#821) - until we implement Client::CreateBucket, simply insert every
    # bucket that the application queries.
    if not gcs_service.has_bucket(bucket_name):
        bucket = gcs_service.GcsBucket(base_url, bucket_name)
        bucket.update_from_metadata({})
        bucket.update_from_metadata({})
        bucket.update_from_metadata({})
        gcs_service.insert_bucket(bucket_name, bucket)
    else:
        bucket = gcs_service.lookup_bucket(bucket_name)
    # end of TODO(#821)
    if bucket is None:
        raise gcs_service.ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    bucket.check_preconditions(flask.request)
    return gcs_service.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['DELETE'])
def buckets_delete(bucket_name):
    """Implement the 'Buckets: delete' API."""
    bucket = gcs_service.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    gcs_service.delete_bucket(bucket_name)
    return gcs_service.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>', methods=['PATCH'])
def buckets_patch(bucket_name):
    """Implement the 'Buckets: patch' API."""
    bucket = gcs_service.lookup_bucket(bucket_name)
    bucket.check_preconditions(flask.request)
    patch = json.loads(flask.request.data)
    bucket.apply_patch(patch)
    return gcs_service.filtered_response(flask.request, bucket.metadata)


@gcs.route('/b/<bucket_name>/acl')
def bucket_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    result = {
        'items': gcs_bucket.metadata.get('acl', []),
    }
    return gcs_service.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/acl', methods=['POST'])
def bucket_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    return gcs_service.filtered_response(flask.request,
                                         gcs_bucket.insert_acl(
                                             payload.get('entity', ''),
                                             payload.get('role', '')))


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['DELETE'])
def bucket_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    gcs_bucket.delete_acl(entity)
    return gcs_service.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/acl/<entity>')
def bucket_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    acl = gcs_bucket.get_acl(entity)
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PUT'])
def bucket_acl_update(bucket_name, entity):
    """Implement the 'BucketAccessControls: update' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_acl(entity, payload.get('role', ''))
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PATCH'])
def bucket_acl_patch(bucket_name, entity):
    """Implement the 'BucketAccessControls: patch' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_acl(entity, payload.get('role', ''))
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl')
def bucket_default_object_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    result = {
        'items': gcs_bucket.metadata.get('defaultObjectAcl', []),
    }
    return gcs_service.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/defaultObjectAcl', methods=['POST'])
def bucket_default_object_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    return gcs_service.filtered_response(flask.request,
                                         gcs_bucket.insert_default_object_acl(
                                             payload.get('entity', ''),
                                             payload.get('role', '')))


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['DELETE'])
def bucket_default_object_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    gcs_bucket.delete_default_object_acl(entity)
    return gcs_service.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>')
def bucket_default_object_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    acl = gcs_bucket.get_default_object_acl(entity)
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PUT'])
def bucket_default_object_acl_update(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: update' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_default_object_acl(entity, payload.get('role', ''))
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PATCH'])
def bucket_default_object_acl_patch(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: patch' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_default_object_acl(entity, payload.get('role', ''))
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/notificationConfigs')
def bucket_notification_list(bucket_name):
    """Implement the 'Notifications: list' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return gcs_service.filtered_response(
        flask.request, {
            'kind': 'storage#notifications',
            'items': gcs_bucket.list_notifications()
        })


@gcs.route('/b/<bucket_name>/notificationConfigs', methods=['POST'])
def bucket_notification_create(bucket_name):
    """Implement the 'Notifications: insert' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    notification = gcs_bucket.insert_notification(flask.request)
    return gcs_service.filtered_response(flask.request, notification)


@gcs.route(
    '/b/<bucket_name>/notificationConfigs/<notification_id>',
    methods=['DELETE'])
def bucket_notification_delete(bucket_name, notification_id):
    """Implement the 'Notifications: delete' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    gcs_bucket.delete_notification(notification_id)
    return gcs_service.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/notificationConfigs/<notification_id>')
def bucket_notification_get(bucket_name, notification_id):
    """Implement the 'Notifications: get' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    notification = gcs_bucket.get_notification(notification_id)
    return gcs_service.filtered_response(flask.request, notification)


@gcs.route('/b/<bucket_name>/iam')
def bucket_get_iam_policy(bucket_name):
    """Implement the 'Buckets: getIamPolicy' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return gcs_service.filtered_response(flask.request,
                                         gcs_bucket.get_iam_policy(
                                             flask.request))


@gcs.route('/b/<bucket_name>/iam', methods=['PUT'])
def bucket_set_iam_policy(bucket_name):
    """Implement the 'Buckets: setIamPolicy' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return gcs_service.filtered_response(flask.request,
                                         gcs_bucket.set_iam_policy(
                                             flask.request))


@gcs.route('/b/<bucket_name>/iam/testPermissions')
def bucket_test_iam_permissions(bucket_name):
    """Implement the 'Buckets: testIamPermissions' API."""
    gcs_bucket = gcs_service.lookup_bucket(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    return gcs_service.filtered_response(flask.request,
                                         gcs_bucket.test_iam_permissions(
                                             flask.request))


@gcs.route('/b/<bucket_name>/o')
def objects_list(bucket_name):
    """Implement the 'Objects: list' API: return the objects in a bucket."""
    result = {'next_page_token': '', 'items': []}
    versions_parameter = flask.request.args.get('versions')
    all_versions = (versions_parameter is not None
                    and bool(versions_parameter))
    for name, o in gcs_service.object_items():
        if name.find(bucket_name + '/o') != 0:
            continue
        if o.get_latest() is None:
            continue
        if all_versions:
            for object_version in o.revisions.itervalues():
                result['items'].append(object_version.metadata)
        else:
            result['items'].append(o.get_latest().metadata)
    return gcs_service.filtered_response(flask.request, result)


@gcs.route(
    '/b/<source_bucket>/o/<source_object>/copyTo/b/<destination_bucket>/o/<destination_object>',
    methods=['POST'])
def objects_copy(source_bucket, source_object, destination_bucket,
                 destination_object):
    """Implement the 'Objects: copy' API, copy an object."""
    object_path, gcs_object = gcs_service.lookup_object(
        source_bucket, source_object)
    gcs_object.check_preconditions(
        flask.request,
        if_generation_match='ifSourceGenerationMatch',
        if_generation_not_match='ifSourceGenerationNotMatch',
        if_metageneration_match='ifSourceMetagenerationMatch',
        if_metageneration_not_match='ifSourceMetagenerationNotMatch')
    source_revision = gcs_object.get_revision(flask.request,
                                              'sourceGeneration')
    if source_revision is None:
        raise gcs_service.ErrorResponse(
            'Revision not found %s' % object_path, status_code=404)

    _, gcs_object = gcs_service.get_object_default(
        destination_bucket, destination_object,
        gcs_service.GcsObject(destination_bucket, destination_object))
    base_url = flask.url_for('gcs_index', _external=True)
    current_version = gcs_object.copy_from(base_url, flask.request,
                                           source_revision)
    gcs_service.insert_object(destination_bucket, destination_object,
                              gcs_object)
    return gcs_service.filtered_response(flask.request,
                                         current_version.metadata)


@gcs.route(
    '/b/<source_bucket>/o/<source_object>/rewriteTo/b/<destination_bucket>/o/<destination_object>',
    methods=['POST'])
def objects_rewrite(source_bucket, source_object, destination_bucket,
                    destination_object):
    """Implement the 'Objects: rewrite' API."""
    base_url = flask.url_for('gcs_index', _external=True)
    gcs_service.GcsBucket.insert_magic_bucket(base_url)
    object_path, gcs_object = gcs_service.lookup_object(
        source_bucket, source_object)
    gcs_object.check_preconditions(
        flask.request,
        if_generation_match='ifSourceGenerationMatch',
        if_generation_not_match='ifSourceGenerationNotMatch',
        if_metageneration_match='ifSourceMetagenerationMatch',
        if_metageneration_not_match='ifSourceMetagenerationNotMatch')
    response = gcs_object.rewrite_step(base_url, flask.request,
                                       destination_bucket, destination_object)
    return gcs_service.filtered_response(flask.request, response)


@gcs.route('/b/<bucket_name>/o/<object_name>')
def objects_get(bucket_name, object_name):
    """Implement the 'Objects: get' API.  Read objects or their metadata."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)

    media = flask.request.args.get('alt', None)
    if media is None or media == 'json':
        return gcs_service.filtered_response(flask.request, revision.metadata)
    if media != 'media':
        raise gcs_service.ErrorResponse('Invalid alt=%s parameter' % media)
    revision.validate_encryption_for_read(flask.request)
    response = flask.make_response(revision.media)
    length = len(revision.media)
    response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1, length)
    return response


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['DELETE'])
def objects_delete(bucket_name, object_name):
    """Implement the 'Objects: delete' API.  Delete objects."""
    object_path, gcs_object = gcs_service.lookup_object(
        bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    remove = gcs_object.del_revision(flask.request)
    gcs_service.delete_object(object_path)

    return gcs_service.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['PUT'])
def objects_update(bucket_name, object_name):
    """Implement the 'Objects: update' API: update an existing Object."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.update_revision(flask.request)
    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>/compose', methods=['POST'])
def objects_compose(bucket_name, object_name):
    """Implement the 'Objects: compose' API: concatenate Objects."""
    payload = json.loads(flask.request.data)
    source_objects = payload["sourceObjects"]
    if source_objects is None:
        raise gcs_service.ErrorResponse(
            'You must provide at least one source component.', status_code=400)
    if len(source_objects) > 32:
        raise gcs_service.ErrorResponse(
            'The number of source components provided'
            ' (%d) exceeds the maximum (32)' % len(source_objects),
            status_code=400)
    composed_media = ""
    for source_object in source_objects:
        source_object_name = source_object.get('name')
        if source_object_name is None:
            raise gcs_service.ErrorResponse('Required.', status_code=400)
        source_object_path, source_gcs_object = gcs_service.lookup_object(
            bucket_name, source_object_name)
        source_revision = source_gcs_object.get_latest()
        generation = source_object.get('generation')
        if generation is not None:
            source_revision = source_gcs_object.get_revision_by_generation(
                generation)
            if source_revision is None:
                raise gcs_service.ErrorResponse(
                    'No such object: %s' % source_object_path, status_code=404)
        object_preconditions = source_object.get('objectPreconditions')
        if object_preconditions is not None:
            if_generation_match = object_preconditions.get('ifGenerationMatch')
            if if_generation_match is not None:
                if source_gcs_object.generation != if_generation_match:
                    raise gcs_service.ErrorResponse(
                        'Precondition Failed', status_code=412)
        composed_media += source_revision.media
    # If the object already exists we need to check the pre-conditions. Find the
    # object with a default value.  But do not insert it yet, the pre-conditions
    # might fail, or the compose might fail and we want to leave the service
    # untouched.
    composed_object_path, gcs_object = gcs_service.get_object_default(
        bucket_name, object_name,
        gcs_service.GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    base_url = flask.url_for('gcs_index', _external=True)
    current_version = gcs_object.compose_from(base_url, flask.request,
                                              composed_media)
    gcs_service.insert_object(bucket_name, object_name, gcs_object)
    return gcs_service.filtered_response(flask.request,
                                         current_version.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['PATCH'])
def objects_patch(bucket_name, object_name):
    """Implement the 'Objects: patch' API: update an existing Object."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.patch_revision(flask.request)
    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl')
def objects_acl_list(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: list' API."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    result = {
        'items': revision.metadata.get('acl', []),
    }
    return gcs_service.filtered_response(flask.request, result)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl', methods=['POST'])
def objects_acl_create(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: create' API."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    return gcs_service.filtered_response(flask.request,
                                         revision.insert_acl(
                                             payload.get('entity', ''),
                                             payload.get('role', '')))


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['DELETE'])
def objects_acl_delete(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: delete' API."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    revision.delete_acl(entity)
    return gcs_service.filtered_response(flask.request, {})


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>')
def objects_acl_get(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: get' API."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    acl = revision.get_acl(entity)
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PUT'])
def objects_acl_update(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: update' API."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    acl = revision.update_acl(entity, payload.get('role', ''))
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PATCH'])
def objects_acl_patch(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: patch' API."""
    _, gcs_object = gcs_service.lookup_object(bucket_name, object_name)
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    acl = revision.patch_acl(entity, flask.request)
    return gcs_service.filtered_response(flask.request, acl)


@gcs.route('/projects/<project_id>/serviceAccount')
def projects_get(project_id):
    """Implement the `Projects.serviceAccount: get` API."""
    return gcs_service.filtered_response(
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


@upload.errorhandler(gcs_service.ErrorResponse)
def upload_error(error):
    return error.as_response()


@upload.route('/b/<bucket_name>/o', methods=['POST'])
def objects_insert(bucket_name):
    """Implement the 'Objects: insert' API.  Insert a new GCS Object."""
    gcs_url = flask.url_for(
        'objects_insert', bucket_name=bucket_name, _external=True).replace(
            '/upload/', '/')
    gcs_service.GcsBucket.insert_magic_bucket(gcs_url)
    object_name = flask.request.args.get('name', None)
    if object_name is None:
        raise gcs_service.ErrorResponse(
            'name not set in Objects: insert', status_code=412)
    upload_type = flask.request.args.get('uploadType')
    if upload_type is None:
        raise gcs_service.ErrorResponse(
            'uploadType not set in Objects: insert', status_code=412)
    if upload_type not in {'multipart', 'media'}:
        raise gcs_service.ErrorResponse(
            'testbench does not support %s uploadType' % upload_type,
            status_code=400)
    object_path, gcs_object = gcs_service.get_object_default(
        bucket_name, object_name,
        gcs_service.GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    gcs_service.insert_object(bucket_name, object_name, gcs_object)
    if upload_type == 'media':
        current_version = gcs_object.insert(gcs_url, flask.request)
    else:
        current_version = gcs_object.insert_multipart(gcs_url, flask.request)
    return gcs_service.filtered_response(flask.request,
                                         current_version.metadata)


# Define the WSGI application to handle (a few) requests in the XML API.
XMLAPI_HANDLER_PATH = '/xmlapi'
xmlapi = flask.Flask(__name__)
xmlapi.debug = True


@xmlapi.errorhandler(gcs_service.ErrorResponse)
def xmlapi_error(error):
    return error.as_response()


@xmlapi.route('/<bucket_name>/<object_name>')
def xmlapi_get_object(bucket_name, object_name):
    """Implement the 'Objects: insert' API.  Insert a new GCS Object."""
    object_path, gcs_object = gcs_service.lookup_object(
        bucket_name, object_name)
    if flask.request.args.get('acl') is not None:
        raise gcs_service.ErrorResponse(
            'ACL query not supported in XML API', status_code=500)
    if flask.request.args.get('encryption') is not None:
        raise gcs_service.ErrorResponse(
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
    gcs_service.GcsBucket.insert_magic_bucket(gcs_url)
    object_path, gcs_object = gcs_service.get_object_default(
        bucket_name, object_name,
        gcs_service.GcsObject(bucket_name, object_name))
    generation_match = flask.request.headers.get('if-generation-match')
    metageneration_match = flask.request.headers.get('if-metageneration-match')
    gcs_object.check_preconditions_by_value(generation_match, None,
                                            metageneration_match, None)
    gcs_service.insert_object(bucket_name, object_name, gcs_object)
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
