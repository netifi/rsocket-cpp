// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/Payload.h"

#include <folly/String.h>
#include <folly/io/Cursor.h>

#include "rsocket/internal/Common.h"

namespace rsocket {

namespace {

std::string moveIOBufToString(std::unique_ptr<folly::IOBuf> buf) {
  return buf ? buf->moveToFbString().toStdString() : "";
}

std::string cloneIOBufToString(std::unique_ptr<folly::IOBuf> const& buf) {
  return buf ? buf->cloneAsValue().moveToFbString().toStdString() : "";
}

} // namespace

Payload::Payload(
    std::unique_ptr<folly::IOBuf> d,
    std::unique_ptr<folly::IOBuf> m)
    : data{std::move(d)}, metadata{std::move(m)} {}

Payload::Payload(folly::StringPiece d, folly::StringPiece m)
    : data{folly::IOBuf::copyBuffer(d.data(), d.size())} {
  if (!m.empty()) {
    metadata = folly::IOBuf::copyBuffer(m.data(), m.size());
  }
}

std::ostream& operator<<(std::ostream& os, const Payload& payload) {
  return os << "Metadata("
            << (payload.metadata ? payload.metadata->computeChainDataLength()
                                 : 0)
            << "): "
            << (payload.metadata ? "'" + humanify(payload.metadata) + "'"
                                 : "<null>")
            << ", Data("
            << (payload.data ? payload.data->computeChainDataLength() : 0)
            << "): "
            << (payload.data ? "'" + humanify(payload.data) + "'" : "<null>");
}

std::string Payload::moveDataToString() {
  return moveIOBufToString(std::move(data));
}

std::string Payload::cloneDataToString() const {
  return cloneIOBufToString(data);
}

std::string Payload::moveMetadataToString() {
  return moveIOBufToString(std::move(metadata));
}

std::string Payload::cloneMetadataToString() const {
  return cloneIOBufToString(metadata);
}

void Payload::clear() {
  data.reset();
  metadata.reset();
}

Payload Payload::clone() const {
  Payload out;
  if (data) {
    out.data = data->clone();
  }
  if (metadata) {
    out.metadata = metadata->clone();
  }
  return out;
}

ErrorWithPayload::ErrorWithPayload(Payload&& payload)
    : payload(std::move(payload)) {}

ErrorWithPayload::ErrorWithPayload(const ErrorWithPayload& oth) {
  payload = oth.payload.clone();
}

ErrorWithPayload& ErrorWithPayload::operator=(const ErrorWithPayload& oth) {
  payload = oth.payload.clone();
  return *this;
}

std::ostream& operator<<(
    std::ostream& os,
    const ErrorWithPayload& errorWithPayload) {
  return os << "rsocket::ErrorWithPayload: " << errorWithPayload.payload;
}

} // namespace rsocket
