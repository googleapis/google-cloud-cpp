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
FROM python:3.8@sha256:ff8ae88abfe6c829c188cc54737139f7c583279009ba34c96034e0e66053ac3f

EXPOSE 9000
WORKDIR /opt/storage-emulator

ADD . /opt/storage-emulator/

RUN python3 -m pip install -r requirements.txt

CMD ["gunicorn", "--bind", "localhost:9000", "--worker-class", "sync", "emulator:run()"]
