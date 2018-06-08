// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENCODER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENCODER_H_

#include "google/cloud/bigtable/internal/strong_type.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * This struct is used to provide the functionality to convert
 * from/to BigEndian numeric value to/from string of bytes.
 *
 * Why this functionality:
 * In Google Cloud Bigtable, values are stored in a cell as a std::string
 * data type only. But still this cell contains string and numeric values.
 * String can be stored into cell as it is, but Numeric values are stored
 * as a 64-bit BigEndian hex bytes converted into string of bytes.
 * For human readability it should be easy to put string and numeric
 * values into the cell. So using functions of this struct we can
 * convert human readable numeric values into string of BigEndian hex
 * bytes and get it back into human readable numeric values.
 *
 * Currently we only support conversion of 64 bit BigEndian values
 * from/to numeric values. This structure can be extended to support
 * other types of data also.
 *
 * How to use for coding:
 * Convert from Numeric Value to string of BigEndian bytes.
 * @code
 * bigtable::Cell new_cell("row_key", "column_family", "column_id3", 1000,
 *                         bigtable::bigendian64_t(5000), {});
 * @endcode
 *
 * Convert from string of BigEndian bytes to Numeric Value.
 * @code
 * std::int64_t cell_value = new_cell.value_as<bigtable::bigendian64_t>().get();
 * @endcode
 *
 * How to use to support new data types:
 * Recommended way is to define your own Strong Type
 * (see: bigtable/internal/strong_type.h) to use with this struct.
 *
 *  // Following code shows how to support new type for Encode and Decode
 *  // functions. Code shows support fot bigtable::bigendian64_t strong type
 *
 *  // Encode function : Convert BigEndian 64 bit hex bytes inside
 *  // strong type bigendian64_t into string of bytes.
 * @code
 * template<>
 * std::string Encoder<bigtable::bigendian64_t>::Encode
 *                               (bigtable::bigendian64_t const& value) {}
 *  // Decode function : Convert BigEndian 64 bit string of bytes into
 *  // strong type bigendian64_t
 * template<>
 * bigtable::bigendian64_t Encoder<bigtable::bigendian64_t>::Decode
 *                               (std::string const& value) {}
 * @endcode
 *
 * Implement above two functions for conversion and use it as mentioned in
 * the above how to use example.
 */
template <typename T>
struct Encoder {
  /**
   * Convert BigEndian numeric value into a string of bytes and return it.
   *
   * Google Cloud Bigtable stores arbitrary blobs in each cell. These
   * blobs are stored in a cell as a string of bytes. Values need to be
   * converted to/from these cell blobs bytes to be used in application.
   * This function is used to convert BigEndian 64 bit numeric value into
   * string of bytes, so that it could be stored as a cell blob.
   * For this conversion we consider the char size is 8 bits.
   * So If machine architecture does not define char as 8 bit then we
   * raise the assert.
   *
   * @param value const reference to a number need to be converted into
   *        string of bytes.
   */
  static std::string Encode(T const& value);

  /**
   * Convert BigEndian string of bytes into BigEndian numeric value and
   * return it.
   *
   * Google Cloud Bigtable stores arbitrary blobs in each cell. These
   * blobs are stored in a cell as a string of bytes. Values need to be
   * converted to/from these cell blobs bytes to be used in application.
   * This function is used to convert string of bytes into BigEndian 64
   * bit numeric value. so that it could be used in application.
   * For this conversion we consider the char size is 8 bits.
   * So If machine architecture does not define char as 8 bit then we
   * raise the std::range_error.
   *
   * @param value reference to a string of bytes need to be converted into
   *        BigEndian numeric value.
   */
  static T Decode(std::string const& value);
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENCODER_H_
