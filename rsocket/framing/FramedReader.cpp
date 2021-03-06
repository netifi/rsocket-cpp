// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FramedReader.h"

#include <folly/io/Cursor.h>

#include "rsocket/framing/FrameSerializer_v1_0.h"

namespace rsocket {

using namespace yarpl::flowable;

namespace {

constexpr size_t kFrameLengthFieldLengthV1_0 = 3;

/// Get the byte size of the frame length field in an RSocket frame.
size_t frameSizeFieldLength(ProtocolVersion version) {
  DCHECK_NE(version, ProtocolVersion::Unknown);
  return kFrameLengthFieldLengthV1_0;
}

/// Get the minimum size for a valid RSocket frame (including its frame length
/// field).
size_t minimalFrameLength(ProtocolVersion version) {
  DCHECK_NE(version, ProtocolVersion::Unknown);
  return FrameSerializerV1_0::kFrameHeaderSize;
}

/// Compute the length of the entire frame (including its frame length field),
/// if given only its frame length field.
size_t frameSizeWithLengthField(ProtocolVersion version, size_t frameSize) {
  return version < FrameSerializerV1_0::Version
      ? frameSize
      : frameSize + frameSizeFieldLength(version);
}

/// Compute the length of the frame (excluding its frame length field), if given
/// only its frame length field.
size_t frameSizeWithoutLengthField(ProtocolVersion version, size_t frameSize) {
  DCHECK_NE(version, ProtocolVersion::Unknown);
  return version < FrameSerializerV1_0::Version
      ? frameSize - frameSizeFieldLength(version)
      : frameSize;
}
} // namespace

size_t FramedReader::readFrameLength() const {
  const auto fieldLength = frameSizeFieldLength(*version_);
  DCHECK_GT(fieldLength, 0);

  folly::io::Cursor cur{payloadQueue_.front()};
  size_t frameLength = 0;

  // Reading of arbitrary-sized big-endian integer.
  for (size_t i = 0; i < fieldLength; ++i) {
    frameLength <<= 8;
    frameLength |= cur.read<uint8_t>();
  }

  return frameLength;
}

void FramedReader::onSubscribe(std::shared_ptr<Subscription> subscription) {
  DuplexConnection::DuplexSubscriber::onSubscribe(subscription);
  subscription->request(std::numeric_limits<int64_t>::max());
}

void FramedReader::onNext(std::unique_ptr<folly::IOBuf> payload) {
  VLOG(4) << "incoming bytes length=" << payload->length() << '\n'
          << hexDump(payload->clone()->moveToFbString());
  payloadQueue_.append(std::move(payload));
  parseFrames();
}

void FramedReader::parseFrames() {
  if (dispatchingFrames_) {
    return;
  }

  // Delivering onNext can trigger termination and destroy this instance.
  auto const self = shared_from_this();

  dispatchingFrames_ = true;

  while (allowance_.canConsume(1) && inner_) {
    if (!ensureOrAutodetectProtocolVersion()) {
      // At this point we dont have enough bytes on the wire or we errored out.
      break;
    }

    auto const frameSizeFieldLen = frameSizeFieldLength(*version_);
    if (payloadQueue_.chainLength() < frameSizeFieldLen) {
      // We don't even have the next frame size value.
      break;
    }

    auto const nextFrameSize = readFrameLength();
    if (nextFrameSize < minimalFrameLength(*version_)) {
      error("Invalid frame - Frame size smaller than minimum");
      break;
    }

    if (payloadQueue_.chainLength() <
        frameSizeWithLengthField(*version_, nextFrameSize)) {
      // Need to accumulate more data.
      break;
    }

    payloadQueue_.trimStart(frameSizeFieldLen);
    const auto payloadSize =
        frameSizeWithoutLengthField(*version_, nextFrameSize);

    DCHECK_GT(payloadSize, 0)
        << "folly::IOBufQueue::split(0) returns a nullptr, can't have that";
    auto nextFrame = payloadQueue_.split(payloadSize);

    CHECK(allowance_.tryConsume(1));

    VLOG(4) << "parsed frame length=" << nextFrame->length() << '\n'
            << hexDump(nextFrame->clone()->moveToFbString());
    inner_->onNext(std::move(nextFrame));
  }

  dispatchingFrames_ = false;
}

void FramedReader::onComplete() {
  payloadQueue_.move();
  DuplexConnection::DuplexSubscriber::onComplete();
  if (auto subscriber = std::move(inner_)) {
    // After this call the instance can be destroyed!
    subscriber->onComplete();
  }
}

void FramedReader::onError(folly::exception_wrapper ex) {
  payloadQueue_.move();
  DuplexConnection::DuplexSubscriber::onError({});
  if (auto subscriber = std::move(inner_)) {
    // After this call the instance can be destroyed!
    subscriber->onError(std::move(ex));
  }
}

void FramedReader::request(int64_t n) {
  allowance_.add(n);
  parseFrames();
}

void FramedReader::cancel() {
  allowance_.consumeAll();
  inner_ = nullptr;
}

void FramedReader::setInput(
    std::shared_ptr<DuplexConnection::Subscriber> inner) {
  CHECK(!inner_)
      << "Must cancel original input to FramedReader before setting a new one";
  inner_ = std::move(inner);
  inner_->onSubscribe(shared_from_this());
}

bool FramedReader::ensureOrAutodetectProtocolVersion() {
  if (*version_ != ProtocolVersion::Unknown) {
    return true;
  }

  const auto minBytesNeeded =
      FrameSerializerV1_0::kMinBytesNeededForAutodetection;
  DCHECK_GT(minBytesNeeded, 0);
  if (payloadQueue_.chainLength() < minBytesNeeded) {
    return false;
  }

  DCHECK_GT(minBytesNeeded, kFrameLengthFieldLengthV1_0);

  auto const& firstFrame = *payloadQueue_.front();

  const auto detectedV1 = FrameSerializerV1_0::detectProtocolVersion(
      firstFrame, kFrameLengthFieldLengthV1_0);
  if (detectedV1 != ProtocolVersion::Unknown) {
    *version_ = FrameSerializerV1_0::Version;
    return true;
  }

  error("Could not detect protocol version from framing");
  return false;
}

void FramedReader::error(std::string errorMsg) {
  VLOG(1) << "error: " << errorMsg;

  payloadQueue_.move();
  if (DuplexConnection::DuplexSubscriber::subscription()) {
    DuplexConnection::DuplexSubscriber::subscription()->cancel();
  }
  if (auto subscriber = std::move(inner_)) {
    // After this call the instance can be destroyed!
    subscriber->onError(std::runtime_error{std::move(errorMsg)});
  }
}
} // namespace rsocket
