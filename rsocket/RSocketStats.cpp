// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketStats.h"

namespace rsocket {

class NoopStats : public RSocketStats {
 public:
  NoopStats() = default;
  NoopStats(const NoopStats&) = delete; // non construction-copyable
  NoopStats& operator=(const NoopStats&) = delete; // non copyable
  NoopStats& operator=(const NoopStats&&) = delete; // non movable
  NoopStats(NoopStats&&) = delete; // non construction-movable
  ~NoopStats() = default;

  void socketCreated() override {}
  void socketConnected() override {}
  void socketDisconnected() override {}
  void socketClosed(StreamCompletionSignal) override {}

  void serverConnectionAccepted() override {}

  void duplexConnectionCreated(const std::string&, rsocket::DuplexConnection*)
      override {}

  void duplexConnectionClosed(const std::string&, rsocket::DuplexConnection*)
      override {}

  void bytesWritten(size_t) override {}
  void bytesRead(size_t) override {}
  void frameWritten(FrameType) override {}
  void frameRead(FrameType) override {}
  virtual void serverResume(
      folly::Optional<int64_t>,
      int64_t,
      int64_t,
      ResumeOutcome) override {}
  void resumeBufferChanged(int, int) override {}
  void streamBufferChanged(int64_t, int64_t) override {}

  void resumeFailedNoState() override {}

  void keepaliveSent() override {}
  void keepaliveReceived() override {}

  static std::shared_ptr<NoopStats> instance() {
    static const auto singleton = std::make_shared<NoopStats>();
    return singleton;
  }
};

std::shared_ptr<RSocketStats> RSocketStats::noop() {
  return NoopStats::instance();
}
} // namespace rsocket
