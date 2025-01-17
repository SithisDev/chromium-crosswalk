// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/deferred_gpu_command_service.h"

#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/task_forwarding_sequence.h"
#include "android_webview/browser/gfx/task_queue_web_view.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager_factory.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"
#include "ui/gl/gl_share_group.h"

namespace android_webview {

// static
DeferredGpuCommandService*
DeferredGpuCommandService::CreateDeferredGpuCommandService() {
  return new DeferredGpuCommandService(TaskQueueWebView::GetInstance(),
                                       GpuServiceWebView::GetInstance());
}

// static
DeferredGpuCommandService* DeferredGpuCommandService::GetInstance() {
  static DeferredGpuCommandService* service = CreateDeferredGpuCommandService();
  return service;
}

DeferredGpuCommandService::DeferredGpuCommandService(
    TaskQueueWebView* task_queue,
    GpuServiceWebView* gpu_service)
    : gpu::CommandBufferTaskExecutor(gpu_service->gpu_preferences(),
                                     gpu_service->gpu_feature_info(),
                                     gpu_service->sync_point_manager(),
                                     gpu_service->mailbox_manager(),
                                     nullptr,
                                     gl::GLSurfaceFormat(),
                                     gpu_service->shared_image_manager(),
                                     nullptr,
                                     nullptr),
      task_queue_(task_queue),
      gpu_service_(gpu_service) {}

DeferredGpuCommandService::~DeferredGpuCommandService() {}

// Called from different threads!
std::unique_ptr<gpu::SingleTaskSequence>
DeferredGpuCommandService::CreateSequence() {
  return std::make_unique<TaskForwardingSequence>(
      this->task_queue_, this->gpu_service_->sync_point_manager());
}

void DeferredGpuCommandService::ScheduleOutOfOrderTask(base::OnceClosure task) {
  task_queue_->ScheduleTask(std::move(task), true /* out_of_order */);
}

void DeferredGpuCommandService::ScheduleDelayedWork(
    base::OnceClosure callback) {
  task_queue_->ScheduleIdleTask(std::move(callback));
}

void DeferredGpuCommandService::PostNonNestableToClient(
    base::OnceClosure callback) {
  task_queue_->ScheduleClientTask(std::move(callback));
}

bool DeferredGpuCommandService::ForceVirtualizedGLContexts() const {
  return true;
}

bool DeferredGpuCommandService::ShouldCreateMemoryTracker() const {
  return false;
}

bool DeferredGpuCommandService::CanSupportThreadedTextureMailbox() const {
  return gpu_info().can_support_threaded_texture_mailbox;
}

}  // namespace android_webview
