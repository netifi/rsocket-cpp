// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include <sstream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "yarpl/Flowable.h"

using namespace rsocket;
using namespace yarpl::flowable;

DEFINE_int32(port, 9898, "port to connect to");

class HelloChannelRequestResponder : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  std::shared_ptr<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      std::shared_ptr<Flowable<rsocket::Payload>> request,
      rsocket::StreamId) override {
    std::cout << "Initial request " << initialPayload.cloneDataToString()
              << std::endl;

    // say "Hello" to each name on the input stream
    return request->map([](Payload p) {
      std::cout << "Request Stream: " << p.cloneDataToString() << std::endl;
      std::stringstream ss;
      ss << "Hello " << p.moveDataToString() << "!";
      std::string s = ss.str();
      return Payload(s);
    });
  }
};

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress("::", FLAGS_port);
  opts.threads = 2;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  auto* rawRs = rs.get();
  auto serverThread = std::thread([rawRs] {
    // start accepting connections
    rawRs->startAndPark([](const rsocket::SetupParameters&) {
      return std::make_shared<HelloChannelRequestResponder>();
    });
  });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}
