// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_PARAMS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_PARAMS_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"

namespace notifications {

// Struct used to schedule a notification.
struct NotificationParams {
  NotificationParams(SchedulerClientType type,
                     NotificationData notification,
                     ScheduleParams schedule_params);
  ~NotificationParams();

  // The type of notification using the scheduling system.
  SchedulerClientType type;

  // An auto generated unique id of the scheduled notification.
  std::string guid;

  // Data used to show the notification, such as text or title on the
  // notification.
  NotificationData notification_data;

  // Scheduling details used to determine when to show the notification.
  ScheduleParams schedule_params;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_PARAMS_H_
