// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/benchmarks/throughput_result.h"

namespace google {
namespace cloud {
namespace storage_benchmarks {

void PrintAsCsv(std::ostream& os, ThroughputResult const& r) {
  os << ToString(r.op) << ',' << r.object_size << ',' << r.app_buffer_size
     << ',' << r.lib_buffer_size << ',' << r.crc_enabled << ',' << r.md5_enabled
     << ',' << ToString(r.api) << ',' << r.elapsed_time.count() << ','
     << r.cpu_time.count() << ',' << r.status << '\n';
}

void PrintThroughputResultHeader(std::ostream& os) {
  os << "Op,ObjectSize,AppBufferSize,LibBufferSize"
     << ",Crc32cEnabled,MD5Enabled,ApiName"
     << ",ElapsedTimeUs,CpuTimeUs,Status\n";
}

char const* ToString(OpType op) {
  switch (op) {
    case kOpRead0:
      return "READ[0]";
    case kOpRead1:
      return "READ[1]";
    case kOpRead2:
      return "READ[2]";
    case kOpWrite:
      return "WRITE";
    case kOpInsert:
      return "INSERT";
  }
  return nullptr;  // silence g++ error.
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
