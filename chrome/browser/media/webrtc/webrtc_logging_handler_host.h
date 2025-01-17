// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOGGING_HANDLER_HOST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOGGING_HANDLER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/rtp_dump_type.h"
#include "chrome/browser/media/webrtc/webrtc_text_log_handler.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

class WebRtcLogUploader;
class WebRtcRtpDumpHandler;

namespace content {
class BrowserContext;
}  // namespace content

struct WebRtcLogPaths {
  base::FilePath log_path;  // todo: rename to directory.
  base::FilePath incoming_rtp_dump;
  base::FilePath outgoing_rtp_dump;
};

typedef std::map<std::string, std::string> MetaDataMap;

// WebRtcLoggingHandlerHost handles operations regarding the WebRTC logging:
// - Opens a connection to a WebRtcLoggingAgent that runs in the render process
//   and generates log messages.
// - Writes basic machine info to the log.
// - Informs the handler in the render process when to stop logging.
// - Closes the connection to the WebRtcLoggingAgent (and thereby discarding it)
//   or triggers uploading of the log.
// - Detects when the agent (e.g., because of a tab closure or crash) is going
//   away and possibly triggers uploading the log.
class WebRtcLoggingHandlerHost : public base::SupportsUserData::Data,
                                 public chrome::mojom::WebRtcLoggingClient {
 public:
  typedef base::Callback<void(bool, const std::string&)> GenericDoneCallback;
  typedef base::Callback<void(bool, const std::string&, const std::string&)>
      UploadDoneCallback;
  typedef base::Callback<void(const std::string&, const std::string&)>
      LogsDirectoryCallback;
  typedef base::Callback<void(const std::string&)> LogsDirectoryErrorCallback;

  // Argument #1: Indicate success/failure.
  // Argument #2: If success, the log's ID. Otherwise, empty.
  // Argument #3: If failure, the error message. Otherwise, empty.
  typedef base::RepeatingCallback<
      void(bool, const std::string&, const std::string&)>
      StartEventLoggingCallback;

  // Upload failure reasons used for UMA stats. A failure reason can be one of
  // those listed here or a response code for the upload HTTP request. The
  // values in this list must be less than 100 and cannot be changed.
  enum UploadFailureReason {
    kInvalidState = 0,
    kStoredLogNotFound = 1,
    kNetworkError = 2,
  };

  static void AttachToRenderProcessHost(content::RenderProcessHost* host,
                                        WebRtcLogUploader* log_uploader);
  static WebRtcLoggingHandlerHost* FromRenderProcessHost(
      content::RenderProcessHost* host);

  // Sets meta data that will be uploaded along with the log and also written
  // in the beginning of the log. Must be called on the IO thread before calling
  // StartLogging.
  void SetMetaData(std::unique_ptr<MetaDataMap> meta_data,
                   const GenericDoneCallback& callback);

  // Opens a log and starts logging. Must be called on the IO thread.
  void StartLogging(const GenericDoneCallback& callback);

  // Stops logging. Log will remain open until UploadLog or DiscardLog is
  // called. Must be called on the IO thread.
  void StopLogging(const GenericDoneCallback& callback);

  // Uploads the text log and the RTP dumps. Discards the local copy. May only
  // be called after text logging has stopped. Must be called on the IO thread.
  void UploadLog(const UploadDoneCallback& callback);

  // Uploads a log that was previously saved via a call to StoreLog().
  // Otherwise operates in the same way as UploadLog.
  void UploadStoredLog(const std::string& log_id,
                       const UploadDoneCallback& callback);

  // Called by WebRtcLogUploader when uploading has finished. Must be called on
  // the IO thread.
  void UploadLogDone();

  // Discards the log and the RTP dumps. May only be called after logging has
  // stopped. Must be called on the IO thread.
  void DiscardLog(const GenericDoneCallback& callback);

  // Stores the log locally using a hash of log_id + security origin.
  void StoreLog(const std::string& log_id, const GenericDoneCallback& callback);

  // May be called on any thread. |upload_log_on_render_close_| is used
  // for decision making and it's OK if it changes before the execution based
  // on that decision has finished.
  void set_upload_log_on_render_close(bool should_upload) {
    upload_log_on_render_close_ = should_upload;
  }

  // Starts dumping the RTP headers for the specified direction. Must be called
  // on the IO thread. |type| specifies which direction(s) of RTP packets should
  // be dumped. |callback| will be called when starting the dump is done.
  // |stop_callback| will be called when StopRtpDump is called.
  void StartRtpDump(RtpDumpType type,
                    const GenericDoneCallback& callback,
                    const content::RenderProcessHost::WebRtcStopRtpDumpCallback&
                        stop_callback);

  // Stops dumping the RTP headers for the specified direction. Must be called
  // on the IO thread. |type| specifies which direction(s) of RTP packet dumping
  // should be stopped. |callback| will be called when stopping the dump is
  // done.
  void StopRtpDump(RtpDumpType type, const GenericDoneCallback& callback);

  // Called when an RTP packet is sent or received. Must be called on the UI
  // thread.
  void OnRtpPacket(std::unique_ptr<uint8_t[]> packet_header,
                   size_t header_length,
                   size_t packet_length,
                   bool incoming);

  // Start remote-bound event logging for a specific peer connection
  // (indicated by its session description's ID).
  // The callback will be posted back, indicating |true| if and only if an
  // event log was successfully started, in which case the first of the string
  // arguments will be set to the log-ID. Otherwise, the second of the string
  // arguments will contain the error message.
  // This function must be called on the UI thread.
  void StartEventLogging(const std::string& session_id,
                         size_t max_log_size_bytes,
                         int output_period_ms,
                         size_t web_app_id,
                         const StartEventLoggingCallback& callback);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Ensures that the WebRTC Logs directory exists and then grants render
  // process access to the 'WebRTC Logs' directory, and invokes |callback| with
  // the ids necessary to create a DirectoryEntry object.
  void GetLogsDirectory(const LogsDirectoryCallback& callback,
                        const LogsDirectoryErrorCallback& error_callback);
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  // chrome::mojom::WebRtcLoggingClient methods:
  void OnAddMessages(
      std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages) override;
  void OnStopped() override;

  base::WeakPtr<WebRtcLoggingHandlerHost> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  WebRtcLoggingHandlerHost(int render_process_id,
                           content::BrowserContext* browser_context,
                           WebRtcLogUploader* log_uploader);
  ~WebRtcLoggingHandlerHost() override;

  void OnAgentDisconnected();

  // Called after stopping RTP dumps.
  void StoreLogContinue(const std::string& log_id,
      const GenericDoneCallback& callback);

  // Writes a formatted log |message| to the |circular_buffer_|.
  void LogToCircularBuffer(const std::string& message);

  void TriggerUpload(const UploadDoneCallback& callback,
                     const base::FilePath& log_directory);

  void StoreLogInDirectory(const std::string& log_id,
                           std::unique_ptr<WebRtcLogPaths> log_paths,
                           const GenericDoneCallback& done_callback,
                           const base::FilePath& directory);

  // A helper for TriggerUpload to do the real work.
  void DoUploadLogAndRtpDumps(const base::FilePath& log_directory,
                              const UploadDoneCallback& callback);

  // Create the RTP dump handler and start dumping. Must be called after making
  // sure the log directory exists.
  void CreateRtpDumpHandlerAndStart(RtpDumpType type,
                                    const GenericDoneCallback& callback,
                                    const base::FilePath& dump_dir);

  // A helper for starting RTP dump assuming the RTP dump handler has been
  // created.
  void DoStartRtpDump(RtpDumpType type, const GenericDoneCallback& callback);

  bool ReleaseRtpDumps(WebRtcLogPaths* log_paths);

  void FireGenericDoneCallback(
      const WebRtcLoggingHandlerHost::GenericDoneCallback& callback,
      bool success,
      const std::string& error_message);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Grants the render process access to the 'WebRTC Logs' directory, and
  // invokes |callback| with the ids necessary to create a DirectoryEntry
  // object. If the |logs_path| couldn't be created or found, |error_callback|
  // is run.
  void GrantLogsDirectoryAccess(
      const LogsDirectoryCallback& callback,
      const LogsDirectoryErrorCallback& error_callback,
      const base::FilePath& logs_path);
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  static base::FilePath GetLogDirectoryAndEnsureExists(
      const base::FilePath& browser_context_directory_path);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<chrome::mojom::WebRtcLoggingClient> receiver_;
  mojo::Remote<chrome::mojom::WebRtcLoggingAgent> logging_agent_;

  // The render process ID this object belongs to.
  const int render_process_id_;

  // A callback that needs to be run from a blocking worker pool and returns
  // the browser context directory path associated with our renderer process.
  base::RepeatingCallback<base::FilePath(void)> log_directory_getter_;

  // Only accessed on the IO thread.
  bool upload_log_on_render_close_;

  // The text log handler owns the WebRtcLogBuffer object and keeps track of
  // the logging state. It is a scoped_refptr to allow posting tasks.
  std::unique_ptr<WebRtcTextLogHandler> text_log_handler_;

  // The RTP dump handler responsible for creating the RTP header dump files.
  std::unique_ptr<WebRtcRtpDumpHandler> rtp_dump_handler_;

  // The callback to call when StopRtpDump is called.
  content::RenderProcessHost::WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;

  // A pointer to the log uploader that's shared for all browser contexts.
  // Ownership lies with the browser process.
  WebRtcLogUploader* const log_uploader_;

  // Web app id used for statistics. Created as the hash of the value of a
  // "client" meta data key, if exists. 0 means undefined, and is the hash of
  // the empty string. Must only be accessed on the IO thread.
  int web_app_id_ = 0;

  base::WeakPtrFactory<WebRtcLoggingHandlerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerHost);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOGGING_HANDLER_HOST_H_
