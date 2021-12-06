// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_CONSTANTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_CONSTANTS_H

namespace google {
namespace cloud {
namespace storage {
namespace testing {
// This is a base64-encoded p12 key-file. The service account was deleted
// after creating the key-file, so the key was effectively invalidated, but
// the format is correct, so we can use it to verify that p12 key-files can be
// loaded.
//
// If you want to change the file (for whatever reason), these commands are
// helpful:
//    Generate a new key:
//      gcloud iam service-accounts keys create /dev/shm/key.p12
//          --key-file-type=p12 --iam-account=${SERVICE_ACCOUNT}
//    Base64 encode the key (then cut&paste) here:
//      openssl base64 -e < /dev/shm/key.p12
//    Find the service account ID:
//      openssl pkcs12 -in /dev/shm/key.p12
//          -passin pass:notasecret  -nodes -info  | grep =CN
//    Delete the service account ID:
//      gcloud iam service-accounts delete --quiet ${SERVICE_ACCOUNT}
char const kP12ServiceAccountId[] = "104849618361176160538";
char const kP12KeyFileContents[] =
    "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";

// This is an invalidated private key. It was created using the Google Cloud
// Platform console, but then the key (and service account) were deleted.
auto constexpr kWellFormatedKey = R"""(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S
tTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a
6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/
fS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN
eheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP
T4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U
gyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT
Pg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD
2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB
Sqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov
9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG
DiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX
Z23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC
+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2
UimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r
9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5
3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp
Nx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78
LkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des
AgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk
MGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc
W7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe
Mmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7
rE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3
YvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I
lUtj+/nH3HDQjM4ltYfTPUg=
-----END PRIVATE KEY-----
)""";

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_CONSTANTS_H
