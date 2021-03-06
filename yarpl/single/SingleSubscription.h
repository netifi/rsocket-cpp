// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Refcounted.h"

namespace yarpl {
namespace single {

class SingleSubscription {
 public:
  virtual ~SingleSubscription() = default;
  virtual void cancel() = 0;

 protected:
  SingleSubscription() {}
};

} // namespace single
} // namespace yarpl
