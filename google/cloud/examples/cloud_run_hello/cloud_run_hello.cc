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

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
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
  explicit http_handler() {}

  template <typename Body, typename Fields>
  be::http::response<be::http::string_body> handle_request(
      be::http::request<Body, Fields> request) try {
    // Respond to any request with a "Hello World" message.
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
  be::http::response<be::http::string_body> internal_error(
      be::http::request<Body, Fields> const& request, std::string_view text) {
    be::http::response<be::http::string_body> res{status, request.version()};
    res.set(be::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(be::http::field::content_type, "text/plain");
    res.keep_alive(request.keep_alive());
    res.body() = std::string(text);
    res.prepare_payload();
    return res;
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
       "set the number of I/O threads");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

  auto address = asio::ip::make_address(vm["address"].as<std::string>());
  auto threads = vm["threads"].as<int>();

  std::cout << "Listening on " << address << ":" << port << " using " << threads
            << " threads\n"
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

  auto acc = std::make_shared<acceptor>(
      ioc, tcp::endpoint{address, port},
      std::make_shared<http_handler>());
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
