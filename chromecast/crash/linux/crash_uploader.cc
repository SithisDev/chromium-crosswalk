// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/resource.h>

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/crash/linux/minidump_uploader.h"
#include "chromecast/public/cast_sys_info.h"
#include "chromecast/system/reboot/reboot_util.h"

namespace {

// Upload crash dump for every 60 seconds.
const int kUploadRetryIntervalDefault = 60;

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  chromecast::RegisterPathProvider();
  logging::InitLogging(logging::LoggingSettings());

  LOG(INFO) << "Starting crash uploader...";

  // Nice +19.  Crash uploads are not time critical and we don't want to
  // interfere with user playback.
  setpriority(PRIO_PROCESS, 0, 19);

  // Create the main task executor.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePump::Type::IO);

  std::unique_ptr<chromecast::CastSysInfo> sys_info =
      chromecast::CreateSysInfo();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string server_url(
      command_line->GetSwitchValueASCII(switches::kCrashServerUrl));
  chromecast::MinidumpUploader uploader(sys_info.get(), server_url);
  while (true) {
    if (!uploader.UploadAllMinidumps())
      LOG(ERROR) << "Failed to process minidumps";

    if (uploader.reboot_scheduled())
      chromecast::RebootUtil::RebootNow(
          chromecast::RebootShlib::CRASH_UPLOADER);

    base::PlatformThread::Sleep(
        base::TimeDelta::FromSeconds(kUploadRetryIntervalDefault));
  }

  return EXIT_SUCCESS;
}
