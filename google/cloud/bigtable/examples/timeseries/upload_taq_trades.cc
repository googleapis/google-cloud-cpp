// Copyright 2018 Google LLC
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

#include "circular_buffer.h"
#include "taq.pb.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iomanip>
#include <sstream>

/**
 * @file
 *
 * Shows how to load data into Cloud Bigtable using BulkApply().
 *
 * We use US Equities Market data in this example.  The data is available from:
 *
 * SRC="ftp://ftp.nyxdata.com/Historical Data Samples/Daily TAQ Sample/"
 *
 * In particular we load all the quotes in:
 *
 * DATA=${SRC}/EQY_US_ALL_TRADE_20161024.gz
 *
 * Note that the data is updated from time to time and you may need to use a
 * different date.
 *
 * Each line in this file represents a trade in the US equity markets.
 * The data in this file is compressed, it contains over 27 million trades, with
 * about 2GiB of data once uncompressed.
 *
 * The data is sorted by ticker (aka symbol, aka stock, aka security
 * identifier).  We re-sort the data by timestamp, to represent the order that
 * most real-time applications would face:
 *
 * curl "${DATA}" | gunzip - |
 *     awk 'NR<1 {print $0; next}
 *         /^END/ {print $0; next}
 *         {print $0 | "sort -n"}' >TRADE.sorted.txt
 *
 * Once processed like this, the output file is an ASCII text file, with fields
 * separated by '|' characters, in other words, it is a CSV file with an
 * uncommon separator.  For example the file might contain, with some columns
 * ommitted:
 *
 * @verbatim
 * Time|Exchange|Symbol|Sale Condition|Trade Volume|Trade Price|...
 * 093000417837000|Z|A|@|300|45.88|N|00|1231||C||093000417230000||0
 * 093001004985000|N|A| O  |10017|45.91|N|00|1329||C||093001003301000||0
 * 093002099950000|D|A| 4 B|100|45.91|N|00|1655||C|N|093002000000000||1
 * ...
 * END|20161024|27052068|||||||||||||
 * @endverbatim
 *
 * The `parse_taq_line()` function provides a better documentation of this file
 * format.
 *
 * We will first upload this data to a table where each row corresponds to a
 * line in the input file.  The row key will be `${Symbol}#${YYYYMMDD}#${Time}`,
 * where `Symbol` and `Time` are the values from the corresponding columns, and
 * `YYYYMMDD` is the date implied by the filename. The table will have two
 * column families, as follows:
 *
 * - The `parsed` family contains the data parsed into Protobufs, with one
 *   column:
 *   - `trade` will contain the `Exchange`, `Trade Volume`, and `Trade Price`,
 *      `Sale Condition`, `Sequence Number`, and `Trade Correction Indicator`
 *      fields from the file stored in a `taq.Trade` proto.
 * - The `raw` family also two columns:
 *   - `lineno` contains the line number (in ASCII format) in the original input
 *      file
 *   - `text` contains the original input line.
 *
 * This table is always called `taq-trades-YYYYMMDD` where YYYYMMDD is the date
 * implied by the original source file.
 */

namespace {
namespace cbt = google::cloud::bigtable;

struct TaqLine {
  std::string row_key;
  int lineno;
  std::string text;
  taq::Trade trade;
};

TaqLine parse_taq_line(std::string const& yyyymmdd, int lineno,
                       std::string line);

using CircularBuffer = bigtable_demos::CircularBuffer<cbt::BulkMutation>;
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Make sure we have the right number of arguments.
  if (argc != 5) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <yyyymmdd> <file>" << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const yyyymmdd = argv[3];
  std::string const filename = argv[4];

  // The name of the output table.
  std::string const table_id = "taq-trades";

  // Create the table, the program aborts if the table already exists.
  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions()),
      instance_id);
  auto gc = cbt::GcRule::MaxNumVersions(1);
  // These magical splits are "known" to be good splits for US market data.
  // The shuffle_taq_nbbo program shows how to compute one of these splits.
  std::vector<std::string> splits{
      "AG", "AS", "BH", "CA", "CM", "CT", "DK", "EF", "EW", "FI", "FX",
      "GP", "HR", "IN", "JC", "LA", "MA", "MR", "NO", "OR", "PN", "QS",
      "SA", "SM", "SS", "TG", "TV", "US", "VO", "WL", "XL",
  };
  try {
    auto table_schema = admin.CreateTable(
        table_id, cbt::TableConfig({{"parsed", gc}, {"raw", gc}}, splits));
  } catch (std::exception const&) {
    // Ignore exception because they often happen because the table already
    // exists.
    // TODO(#119) - fix the code here to ignore only the right exception.
    std::cout << "Output table already exists\n";
  }

  cbt::ClientOptions options;
  options.SetLoadBalancingPolicyName("round_robin");

  // Create a connection to Cloud Bigtable and an object to manipulate the
  // specific table used in this demo.
  cbt::Table table(
      cbt::CreateDefaultDataClient(project_id, instance_id, options), table_id);

  // How often do we print a progress message.
  int const report_progress_rate = 100000;
  // The maximum size of a bulk apply.
  int const bulk_apply_size = 10000;
  // The size of the circular buffer.
  int const buffer_size = 1000;
  // The size of the thread pool pushing data to Cloud Bigtable
  int const thread_pool_size = 8;

  // Create a circular buffer to communicate between the main thread that reads
  // the file and the threads that upload the parsed lines to Cloud Bigtable.
  CircularBuffer buffer(buffer_size);
  // Then create a few threads, each one of which pulls mutations out of the
  // circular buffer and then applies the mutation to the table.
  auto read_buffer = [&buffer, &table]() {
    cbt::BulkMutation mutation;
    while (buffer.Pop(mutation)) {
      table.BulkApply(std::move(mutation));
    }
  };
  std::vector<std::future<void>> workers;
  for (int i = 0; i != thread_pool_size; ++i) {
    workers.push_back(std::async(std::launch::async, read_buffer));
  }

  // The main thread just reads the file one line at a time.
  std::ifstream is(filename);
  std::string line;
  // Skip the header line from the input file.
  std::getline(is, line, '\n');
  std::string expected = "Time|Exchange|Symbol|";
  if (line.substr(0, expected.size()) != expected) {
    std::cerr << "Upload aborted because header line <" << line
              << "> does not start with expected fields <" << expected << ">"
              << std::endl;
  }

  std::cout << "Start reading input file " << std::flush;
  auto start = std::chrono::steady_clock::now();

  int lineno = 0;
  cbt::BulkMutation bulk;
  int count = 0;
  while (not is.eof()) {
    ++lineno;
    std::getline(is, line, '\n');
    if (line.substr(0, 4) == "END|") {
      break;
    }
    auto parsed = parse_taq_line(yyyymmdd, lineno, std::move(line));

    cbt::SingleRowMutation mutation(parsed.row_key);
    std::chrono::milliseconds ts(0);
    mutation.emplace_back(
        cbt::SetCell("parsed", "trade", ts, parsed.trade.SerializeAsString()));
    mutation.emplace_back(
        cbt::SetCell("raw", "text", ts, std::move(parsed.text)));
    mutation.emplace_back(
        cbt::SetCell("raw", "lineno", ts, std::to_string(lineno)));
    bulk.emplace_back(std::move(mutation));
    if (++count > bulk_apply_size) {
      buffer.Push(std::move(bulk));
      bulk = {};
      count = 0;
    }

    if (lineno % report_progress_rate == 0) {
      std::cout << "." << std::flush;
    }
  }
  if (count != 0) {
    buffer.Push(std::move(bulk));
  }
  // Let the workers know that they can exit if they find an
  buffer.Shutdown();

  std::cout << "***" << std::flush;

  int worker_count = 0;
  for (auto& task : workers) {
    // If there was an exception in any thread continue, and report any
    // exceptions raised by other threads too.
    try {
      task.get();
    } catch (...) {
      std::cerr << "Exception raised by worker " << worker_count << std::endl;
    }
    ++worker_count;
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << " DONE in " << elapsed.count() << "s" << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
TaqLine parse_taq_line(std::string const& yyyymmdd, int lineno,
                       std::string line) try {
  TaqLine result;

  // The data is in pipe separated fields, we extract them one at a time using
  // a std::istringstream.
  std::istringstream tokens(line);
  // Raise an exception on errors.
  tokens.exceptions(std::ios::failbit);

  // Time: in HHMMSSNNNNNNNNN format (hours, minutes, seconds, nanoseconds).
  std::string tk;
  std::getline(tokens, tk, '|');  // fetch the timestamp as a string

  // ... parse the timestamp into a 64-bit long ...
  if (tk.length() != 15U) {
    std::ostringstream os;
    os << "Time field (" << tk << ") is not in HHMMSSNNNNNNNNN format";
    throw std::runtime_error(os.str());
  }
  std::string timestamp = std::move(tk);

  // Exchange: a single character, US exchanges are identified by a single
  // letter, for example, 'Q' is Nasdaq, 'N' is NYSE, etc.  See
  //   https://www.nyse.com/publicdocs/ctaplan/notifications/trader-update/cqs_output_spec.pdf
  // for details.  In this example we treat all these identifiers as opaque
  // numbers.
  std::getline(tokens, tk, '|');
  result.trade.set_trade_exchange_code(tk[0]);

  // Symbol: string, the security (aka Ticker, aka Symbol) being quoted.
  std::getline(tokens, tk, '|');
  result.row_key = tk + "#" + yyyymmdd + "#" + timestamp;

  // The following fields represent the trade.
  std::getline(tokens, tk, '|');  // Sale Condition: ignored
  // Trade Volume: int32 number of shares in the transaction
  std::getline(tokens, tk, '|');
  result.trade.set_trade_volume(std::stoi(tk));
  // Trade Price: float
  std::getline(tokens, tk, '|');
  result.trade.set_trade_price(std::stod(tk));

  // ... the TAQ line has many other fields that we ignore in this demo ...
  // std::getline(tokens, tk, '|'); // Trade Stop Stock Indicator: ignored
  // std::getline(tokens, tk, '|'); // Trade Correction Indicator: ignored
  // std::getline(tokens, tk, '|'); // Sequence Number: ignored
  // std::getline(tokens, tk, '|'); // Trade Id
  // std::getline(tokens, tk, '|'); // Source of Trade
  // std::getline(tokens, tk, '|'); // Trade Reporting Facility (TRF)
  // std::getline(tokens, tk, '|'); // TRF Participant
  // std::getline(tokens, tk, '|'); // TRF Trade Timestamp

  result.lineno = lineno;
  result.text = std::move(line);
  return result;
} catch (std::exception const& ex) {
  std::ostringstream os;
  os << ex.what() << " in line #" << lineno << " (" << line << ")";
  throw std::runtime_error(os.str());
}

}  // anonymous namespace
