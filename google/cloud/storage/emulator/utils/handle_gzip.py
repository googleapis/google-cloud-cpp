# Copyright 2021 Google LLC
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

import io
import gzip
from werkzeug.wrappers import Request


class HandleGzipMiddleware:
    """
    Handle decompressing requests which contain header Content-Encoding: gzip
    """

    def __init__(self, app):
        self.app = app

    def __call__(self, environ, start_response):
        request = Request(environ)
        if request.headers.get("Content-Encoding", "") == "gzip":
            request.data = gzip.decompress(request.data)
            request.environ["wsgi.input"] = io.BytesIO(request.data)
        return self.app(environ, start_response)
