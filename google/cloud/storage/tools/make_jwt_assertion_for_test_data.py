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
"""Create a JWT assertion for test purposes.

Requires package `python-jose`. This script was written/tested on Python 2.7.

Usage: Change any parameters in the contents dictionary below as necessary, then
simply run this script with no arguments to dump the JWT assertion string.
"""

from jose import jwk
from jose import jws


# Make sure the values in this section are identical to the values in
# service_account_credentials_test.cc.
############################################
# pylint: disable=line-too-long

# The private key JSON below was obtained by downloading a service account
# private key file from the Google Cloud Console, then deleting/invalidating
# that key (along with replacing the other identifiable attributes here).
CONTENTS_DICT = {
    "type": "service_account",
    "project_id": "foo-project",
    "private_key_id": "a1a111aa1111a11a11a11aa111a111a1a1111111",
    "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S\ntTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a\n6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/\nfS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN\neheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP\nT4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U\ngyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT\nPg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD\n2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB\nSqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov\n9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG\nDiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX\nZ23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC\n+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2\nUimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r\n9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5\n3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp\nNx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78\nLkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des\nAgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk\nMGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc\nW7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe\nMmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7\nrE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3\nYvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I\nlUtj+/nH3HDQjM4ltYfTPUg=\n-----END PRIVATE KEY-----\n",
    "client_email": "foo-email@foo-project.iam.gserviceaccount.com",
    "client_id": "100000000000000000001",
    "auth_uri": "https://accounts.google.com/o/oauth2/auth",
    "token_uri": "https://oauth2.googleapis.com/token",
    "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
    "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/foo-email%40foo-project.iam.gserviceaccount.com"
}

# Timestamp used to represent the current time in the JWT assertion.
TIMESTAMP = 1530060324

# Scopes that the returned access token should be valid for use with. This is
# also used in constructing the JWT assertion.
SCOPE_STR = "https://www.googleapis.com/auth/cloud-platform"

ALT_SCOPE_STR = "https://www.googleapis.com/auth/devstorage.full_control";

# pylint: enable=line-too-long
############################################


def ordered_json_str(ordered_dict):
    """Dump a dict to a minified JSON string with ordered keys."""
    kv_strs = []
    # Keys must be iterated over in alphabetical order in order to have
    # deterministic string dump functionality.
    for key, val in sorted(ordered_dict.items()):
        kv_strs.append('"{k}":{q}{v}{q}'.format(
            k=key,
            v=val,
            # Numeric values don't have quotes around them. Note that this falls
            # apart for JSON objects or arrays, but these keyfile attributes
            # only map to strings and ints, so we can take this shortcut.
            q=('' if isinstance(val, int) else '"')
        ))
    return "{" + ",".join(kv_strs) + "}"


def payload_str(scopes, subject=None):
    payload_dict = {
        "aud": CONTENTS_DICT["token_uri"],
        "exp": TIMESTAMP + 3600,
        "iat": TIMESTAMP,
        "iss": CONTENTS_DICT["client_email"],
        "scope": scopes,
    }
    if subject:
        payload_dict["sub"] = subject
    return jwk.base64url_encode(ordered_json_str(payload_dict))

def main():
    """Print out the JWT assertion."""
    signing_algorithm_str = "RS256"  # RSA
    headers_str = jwk.base64url_encode(
        ordered_json_str({
            "alg": signing_algorithm_str,
            "kid": CONTENTS_DICT["private_key_id"],
            "typ": "JWT",
        })
    )

    payload_str_for_defaults = payload_str(SCOPE_STR)
    print("Assertion for default scope and no subject:")
    print(jws._sign_header_and_claims(  # pylint: disable=protected-access
        headers_str,
        payload_str_for_defaults,
        signing_algorithm_str,
        CONTENTS_DICT["private_key"]
    ))

    print()

    payload_str_for_nondefaults = payload_str(ALT_SCOPE_STR, "user@foo.bar")
    print("Assertion for non-default scope and using a subject:")
    print(jws._sign_header_and_claims(  # pylint: disable=protected-access
        headers_str,
        payload_str_for_nondefaults,
        signing_algorithm_str,
        CONTENTS_DICT["private_key"]
    ))


if __name__ == "__main__":
    main()
