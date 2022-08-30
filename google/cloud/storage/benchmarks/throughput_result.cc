// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/benchmarks/throughput_result.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/benchmarks/throughput_options.h"
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace storage_benchmarks {

namespace {

std::string CleanupCsv(std::string v) {
  std::replace(v.begin(), v.end(), ',', ';');
  std::replace(v.begin(), v.end(), '\n', ';');
  std::replace(v.begin(), v.end(), '\r', ';');
  return v;
}

}  // namespace

void PrintAsCsv(std::ostream& os, ThroughputOptions const& options,
                ThroughputResult const& r) {
  auto const start = FormatTimestamp(r.start);

  os << start                                  // clang-format hack
     << ',' << CleanupCsv(options.labels)      //
     << ',' << ToString(r.library)             //
     << ',' << ToString(r.transport)           //
     << ',' << ToString(r.op)                  //
     << ',' << r.object_size                   //
     << ',' << r.transfer_offset               //
     << ',' << r.transfer_size                 //
     << ',' << r.app_buffer_size               //
     << ',' << r.crc_enabled                   //
     << ',' << r.md5_enabled                   //
     << ',' << r.elapsed_time.count()          //
     << ',' << r.cpu_time.count()              //
     << ',' << CleanupCsv(r.peer)              //
     << ',' << CleanupCsv(r.bucket_name)       //
     << ',' << CleanupCsv(r.object_name)       //
     << ',' << CleanupCsv(r.generation)        //
     << ',' << CleanupCsv(r.upload_id)         //
     << ',' << CleanupCsv(r.retry_count)       //
     << ',' << r.status.code()                 //
     << ',' << CleanupCsv(r.status.message())  //
     << '\n';
}

void PrintThroughputResultHeader(std::ostream& os) {
  os << "Start,Labels,Library,Transport,Op,ObjectSize,TransferOffset"
     << ",TransferSize,AppBufferSize,Crc32cEnabled,MD5Enabled"
     << ",ElapsedTimeUs,CpuTimeUs,Peer,BucketName,ObjectName,Generation"
     << ",UploadId,RetryCount,StatusCode,Status\n";
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
