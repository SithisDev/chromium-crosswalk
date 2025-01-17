// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/schedule_params.h"

namespace notifications {

ScheduleParams::ScheduleParams() : priority(Priority::kLow) {}

ScheduleParams::ScheduleParams(const ScheduleParams& other) = default;

bool ScheduleParams::operator==(const ScheduleParams& other) const {
  return priority == other.priority &&
         impression_mapping == other.impression_mapping;
}

ScheduleParams::~ScheduleParams() = default;

}  // namespace notifications
