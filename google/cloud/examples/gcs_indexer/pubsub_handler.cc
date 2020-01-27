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

#include "gcs_indexer_constants.h"
#include <google/cloud/spanner/client.h>
#include <google/cloud/spanner/database_admin_client.h>
#include <google/cloud/spanner/timestamp.h>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

namespace be = boost::beast;
namespace asio = boost::asio;
namespace po = boost::program_options;
namespace pt = boost::property_tree;
namespace spanner = google::cloud::spanner;
using tcp = boost::asio::ip::tcp;

inline constexpr int max_mutations = 1000;
inline constexpr std::uint64_t KiB = 1024;
inline constexpr auto request_body_size_limit = 32 * KiB;
inline constexpr auto request_timeout = std::chrono::seconds(30);

using row_type = std::tuple<std::string, std::string, std::string, bool,
                            spanner::Timestamp, spanner::Timestamp>;
using primary_key = std::tuple<std::string, std::string, std::string>;
inline constexpr std::string_view event_finalize = "OBJECT_FINALIZE";
inline constexpr std::string_view event_update = "OBJECT_METADATA_UPDATE";
inline constexpr std::string_view event_delete = "OBJECT_DELETE";
inline constexpr std::string_view event_archive = "OBJECT_ARCHIVE";

// Report a failure
void report_error(be::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

[[noreturn]] void throw_error(be::error_code ec, char const* what) {
  std::ostringstream os;
  os << what << ": " << ec.message() << "\n";
  throw std::runtime_error(os.str());
}

/**
 * Handle a HTTP request.
 */
class http_handler {
 public:
  explicit http_handler(spanner::Client client, spanner::Database database)
      : client_(std::move(client)), database_(std::move(database)) {}

  /**
   * Handle a Google Cloud Storage Pub/Sub notification.
   */
  template <typename Body, typename Fields>
  be::http::response<be::http::string_body> handle_gcs_notification(
      be::http::request<Body, Fields> request) {
    if (request[be::http::field::content_type] != "application/json") {
      return bad_request(request, "invalid content-type for GCS notification");
    }

    std::function<void(std::string const& prefix, pt::ptree const& tree)>
        dump_tree;

    dump_tree = [&](std::string const& prefix, pt::ptree const& tree) {
      for (auto& [k, v] : tree) {
        auto p = prefix;
        p += ".";
        p += k;
        std::cout << p << ":  " << v.data() << "\n";
        auto sub_tree = tree.get_child(k, pt::ptree{});
        dump_tree(p, sub_tree);
      }
    };

    pt::ptree body = [&request] {
      std::istringstream is(request.body());
      pt::ptree tree;
      pt::json_parser::read_json(is, tree);
      return tree;
    }();
    // dump_tree("   body", body);

    pt::ptree payload = [&body] {
      std::string raw = body.get<std::string>("message.data", "");
      namespace bai = boost::archive::iterators;
      using binary_t =
          bai::transform_width<bai::binary_from_base64<std::string::iterator>,
                               8, 6>;
      // Pad the raw string if needed.
      raw.append((4 - raw.size() % 4) % 4, '=');
      std::string data{binary_t(raw.begin()), binary_t(raw.end())};
      // Boost.Property does not like characters after the closing brace.
      data = data.substr(0, data.rfind('}') + 1);
      std::istringstream is(data);
      pt::ptree tree;
      pt::json_parser::read_json(is, tree);
      return tree;
    }();
    // dump_tree("   payload", payload);

    auto const& attributes = body.get_child("message.attributes");

    auto event_type = attributes.get<std::string>("eventType");
    auto payload_format = attributes.get<std::string>("payloadFormat");
    auto bucket = attributes.get<std::string>("bucketId");
    auto object = attributes.get<std::string>("objectId");
    auto generation = attributes.get<std::string>("objectGeneration");

    std::cout << "event_type=" << event_type
              << ", payload_format=" << payload_format << ", path=gs://"
              << bucket << "/" << object << "/" << generation << std::endl;

    // Acknowledge the notification, that frees up the Pub/Sub resources.
    be::http::response<be::http::string_body> success{
        be::http::status::no_content, request.version()};
    success.set(be::http::field::server, BOOST_BEAST_VERSION_STRING);
    success.set(be::http::field::content_type, "application/json");
    success.keep_alive(request.keep_alive());
    success.prepare_payload();

    if (event_type == event_update) {
      return success;
    }

    if (event_type == event_delete) {
      auto mutation = spanner::MakeDeleteMutation(
          std::string(table_name), spanner::KeySet().AddKey(spanner::MakeKey(
                                       bucket, object, generation)));
      client_
          .Commit(
              [m = std::move(mutation)](auto) { return spanner::Mutations{m}; })
          .value();
      return success;
    }

    auto columns = [] {
      return std::vector<std::string>{std::begin(column_names),
                                      std::end(column_names)};
    };

    if (event_type == event_archive || event_type == event_finalize) {
      bool is_archived = event_type == event_archive;

      auto update = spanner::MakeInsertOrUpdateMutation(
          std::string(table_name), columns(), bucket, object, generation,
          payload.get<std::string>("metaGeneration", ""), is_archived,
          payload.get<std::int64_t>("size", 0),
          payload.get<std::string>("contentType", ""),
          spanner::internal::TimestampFromRFC3339(
              payload.get<std::string>("timeCreated"))
              .value(),
          spanner::internal::TimestampFromRFC3339(
              payload.get<std::string>("updated"))
              .value(),
          payload.get<std::string>("storageClass", ""),
          spanner::internal::TimestampFromRFC3339(
              payload.get<std::string>("timeStorageClassUpdated"))
              .value(),
          payload.get<std::string>("md5Hash", ""),
          payload.get<std::string>("crc32c", ""),
          spanner::MakeTimestamp(std::chrono::system_clock::now()).value());
      client_
          .Commit(
              [m = std::move(update)](auto) { return spanner::Mutations{m}; })
          .value();
      return success;
    }

    return bad_request(request, "unknown event type");
  }

  template <typename Body, typename Fields>
  be::http::response<be::http::string_body> handle_request(
      be::http::request<Body, Fields> request) try {
    if (request.method() == be::http::verb::post && request.target() == "/") {
      return handle_gcs_notification(std::move(request));
    }

    if (request.method() != be::http::verb::get) {
      return bad_request(request, "Unknown HTTP-method");
    }

    // Respond to GET request, mostly useful for debugging.
    // TODO(...) - return an error, this is just for debugging.
    be::http::response<be::http::string_body> res{be::http::status::ok,
                                                  request.version()};
    res.set(be::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(be::http::field::content_type, "text/plain");
    res.keep_alive(request.keep_alive());
    res.body() = "Hello World\n";
    res.prepare_payload();
    return res;
  } catch (std::exception const& ex) {
    std::ostringstream os;
    os << "Exception caught in HTTP handler: " << ex.what();
    std::cerr << os.str() << std::endl;
    return internal_error(request, std::move(os).str());
  }

 private:
  template <typename Body, typename Fields>
  be::http::response<be::http::string_body> error_response(
      be::http::request<Body, Fields> const& request, be::http::status status,
      std::string_view text) {
    be::http::response<be::http::string_body> res{status, request.version()};
    res.set(be::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(be::http::field::content_type, "text/plain");
    res.keep_alive(request.keep_alive());
    res.body() = std::string(text);
    res.prepare_payload();
    return res;
  }

  template <typename Body, typename Fields>
  be::http::response<be::http::string_body> bad_request(
      be::http::request<Body, Fields> const& request, std::string_view text) {
    return error_response(request, be::http::status::bad_request, text);
  }

  template <typename Body, typename Fields>
  be::http::response<be::http::string_body> internal_error(
      be::http::request<Body, Fields> const& request, std::string_view text) {
    return error_response(request, be::http::status::internal_server_error,
                          text);
  }

  spanner::Client client_;
  spanner::Database database_;
};

/**
 * Handle a HTTP session.
 */
class http_session : public std::enable_shared_from_this<http_session> {
 public:
  explicit http_session(tcp::socket socket,
                        std::shared_ptr<http_handler> handler)
      : stream_(std::move(socket)), handler_(std::move(handler)) {}

  void start() {
    parser_.emplace();
    parser_->body_limit(request_body_size_limit);
    stream_.expires_after(request_timeout);

    be::http::async_read(
        stream_, buffer_, *parser_,
        [self = shared_from_this()](be::error_code ec, std::size_t s) {
          self->on_read(ec, s);
        });
  }

 private:
  void on_read(be::error_code ec, std::size_t /*bytes_transferred*/) {
    // End of stream is expected, simply close the socket and return.
    if (ec == be::http::error::end_of_stream) return close();

    // Other errors are unexpected, just report the error and let the socket
    // close.
    if (ec) return report_error(ec, "on_read");

    auto send = [this](auto response) {
      // We need to hold the response object until the async_write() below
      // completes. Create an object to hold the response, and make the
      // callback the owner of it.
      using response_type = std::decay_t<decltype(response)>;
      struct pending_response {
        pending_response(std::shared_ptr<http_session> s, response_type r)
            : session(std::move(s)), response(std::move(r)) {}

        std::shared_ptr<http_session> session;
        response_type response;
      };
      auto pr = std::make_shared<pending_response>(shared_from_this(),
                                                   std::move(response));
      be::http::async_write(stream_, pr->response, [pr](auto ec, auto s) {
        pr->session->on_write(pr->response.need_eof(), ec, s);
      });
    };

    send(handler_->handle_request(std::move(parser_->release())));
  }

  void on_write(bool need_eof, be::error_code ec,
                std::size_t /*bytes_transferred*/) {
    if (ec) return report_error(ec, "on_write");
    if (need_eof) close();
    // Read the next request.
    start();
  }

  void close() {
    be::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
  }

  be::tcp_stream stream_;
  std::shared_ptr<http_handler> handler_;
  be::flat_buffer buffer_;

  // The parser is stored in an optional container so we can
  // construct it from scratch it at the beginning of each new message.
  std::optional<be::http::request_parser<be::http::string_body>> parser_;
};

/**
 * Accept HTTP sessions.
 */
class acceptor : public std::enable_shared_from_this<acceptor> {
 public:
  acceptor(asio::io_context& ioc, tcp::endpoint const& endpoint,
           std::shared_ptr<http_handler> handler)
      : ioc_(ioc),
        socket_acceptor_(asio::make_strand(ioc_)),
        handler_(std::move(handler)) {
    be::error_code ec;

    // Open the acceptor
    socket_acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      throw_error(ec, "open");
    }

    // Allow address reuse
    socket_acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
      throw_error(ec, "set_option");
    }

    // Bind to the server address
    socket_acceptor_.bind(endpoint, ec);
    if (ec) {
      throw_error(ec, "bind");
    }

    // Start listening for connections
    socket_acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
      throw_error(ec, "listen");
    }
  }

  void start() {
    socket_acceptor_.async_accept(
        asio::make_strand(ioc_),
        [self = shared_from_this()](be::error_code const& ec, tcp::socket s) {
          self->on_accept(ec, std::move(s));
        });
  }

 private:
  void on_accept(be::error_code ec, tcp::socket socket) {
    if (ec) {
      report_error(ec, "on_accept");
    } else {
      std::make_shared<http_session>(std::move(socket), handler_)->start();
    }
    start();
  }

  asio::io_context& ioc_;
  tcp::acceptor socket_acceptor_;
  std::shared_ptr<http_handler> handler_;
};

int main(int argc, char* argv[]) try {
  auto const default_threads = [] {
    auto count = std::thread::hardware_concurrency();
    return count == 0 ? 1 : static_cast<int>(count);
  }();

  auto get_env = [](std::string_view name) -> std::string {
    auto value = std::getenv(name.data());
    return value == nullptr ? std::string{} : value;
  };

  auto port = [&]() -> std::uint16_t {
    auto env = get_env("PORT");
    if (env.empty()) return 8080;
    auto value = std::stoi(env);
    if (value < std::numeric_limits<std::uint16_t>::min() ||
        value > std::numeric_limits<std::uint16_t>::max()) {
      std::ostringstream os;
      os << "The PORT environment variable value (" << value
         << ") is out of range.";
      throw std::invalid_argument(std::move(os).str());
    }
    return static_cast<std::uint16_t>(value);
  }();

  po::options_description desc("Server configuration");
  desc.add_options()
      //
      ("help", "produce help message")(
          "address", po::value<std::string>()->default_value("0.0.0.0"),
          "set listening address")
      //
      ("port", po::value<std::uint16_t>(&port), "set listening port")
      //
      ("threads", po::value<int>()->default_value(default_threads),
       "set the number of I/O threads")
      //
      ("project",
       po::value<std::string>()->default_value(get_env("GOOGLE_CLOUD_PROJECT")),
       "set the Google Cloud Platform project id")
      //
      ("instance", po::value<std::string>()->required(),
       "set the Cloud Spanner instance id")
      //
      ("database", po::value<std::string>()->required(),
       "set the Cloud Spanner database id");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }
  for (auto arg : {"project", "instance", "database"}) {
    if (vm.count(arg) != 1 || vm[arg].as<std::string>().empty()) {
      std::cout << "The --" << arg
                << " option must be set to a non-empty value\n"
                << desc << "\n";
      return 1;
    }
  }

  auto address = asio::ip::make_address(vm["address"].as<std::string>());
  auto threads = vm["threads"].as<int>();
  spanner::Database database(vm["project"].as<std::string>(),
                             vm["instance"].as<std::string>(),
                             vm["database"].as<std::string>());

  std::cout << "Listening on " << address << ":" << port << " using " << threads
            << " threads\n"
            << "Will update object index in database: " << database
            << std::endl;

  asio::io_context ioc{threads};
  // Capture SIGINT and SIGTERM to perform a clean shutdown
  asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) {
    // Stop the `io_context`. This will cause `run()`
    // to return immediately, eventually destroying the
    // `io_context` and all of the sockets in it.
    ioc.stop();
  });

  spanner::Client client(spanner::MakeConnection(database));

  auto acc = std::make_shared<acceptor>(
      ioc, tcp::endpoint{address, port},
      std::make_shared<http_handler>(std::move(client), std::move(database)));
  acc->start();

  std::vector<std::thread> v(threads - 1);
  std::generate_n(v.begin(), v.size(),
                  [&ioc] { return std::thread([&ioc] { ioc.run(); }); });
  // Block also the main thread, reaching the designed count.
  ioc.run();

  // Block until all the threads exit
  for (auto& t : v) {
    t.join();
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception caught " << ex.what() << '\n';
  return 1;
}
