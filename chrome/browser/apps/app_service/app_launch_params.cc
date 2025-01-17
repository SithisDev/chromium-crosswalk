// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_launch_params.h"

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const std::string& app_id,
                                 apps::mojom::LaunchContainer container,
                                 WindowOpenDisposition disposition,
                                 apps::mojom::AppLaunchSource source,
                                 int64_t display_id)
    : profile(profile),
      app_id(app_id),
      container(container),
      disposition(disposition),
      command_line(base::CommandLine::NO_PROGRAM),
      source(source),
      display_id(display_id),
      opener(nullptr) {}

AppLaunchParams::AppLaunchParams(const AppLaunchParams& other) = default;

AppLaunchParams::~AppLaunchParams() = default;
