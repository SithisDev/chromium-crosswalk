// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/geolocation/geoposition.h"
#include "chromeos/timezone/timezone_provider.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kRequestRetryIntervalMilliSeconds = 200;

// This should be different from default to prevent TimeZoneRequest
// from modifying it.
const char kTestTimeZoneProviderUrl[] =
    "https://localhost/maps/api/timezone/json?";

const char kSimpleResponseBody[] =
    "{\n"
    "    \"dstOffset\" : 0.0,\n"
    "    \"rawOffset\" : -28800.0,\n"
    "    \"status\" : \"OK\",\n"
    "    \"timeZoneId\" : \"America/Los_Angeles\",\n"
    "    \"timeZoneName\" : \"Pacific Standard Time\"\n"
    "}";

struct SimpleRequest {
  SimpleRequest()
      : url("https://localhost/maps/api/timezone/"
            "json?location=39.603481,-119.682251&timestamp=1331161200&sensor="
            "false"),
        http_response(kSimpleResponseBody) {
    position.latitude = 39.6034810;
    position.longitude = -119.6822510;
    position.accuracy = 1;
    position.error_code = 0;
    position.timestamp = base::Time::FromTimeT(1331161200);
    position.status = chromeos::Geoposition::STATUS_NONE;
    EXPECT_EQ(
        "latitude=39.603481, longitude=-119.682251, accuracy=1.000000, "
        "error_code=0, error_message='', status=0 (NONE)",
        position.ToString());

    timezone.dstOffset = 0;
    timezone.rawOffset = -28800;
    timezone.timeZoneId = "America/Los_Angeles";
    timezone.timeZoneName = "Pacific Standard Time";
    timezone.error_message.erase();
    timezone.status = chromeos::TimeZoneResponseData::OK;
    EXPECT_EQ(
        "dstOffset=0.000000, rawOffset=-28800.000000, "
        "timeZoneId='America/Los_Angeles', timeZoneName='Pacific Standard "
        "Time', error_message='', status=0 (OK)",
        timezone.ToStringForDebug());
  }

  GURL url;
  chromeos::Geoposition position;
  std::string http_response;
  chromeos::TimeZoneResponseData timezone;
};

}  // anonymous namespace

namespace chromeos {

// This implements fake TimeZone API remote endpoint.
class TestTimeZoneAPILoaderFactory : public network::TestURLLoaderFactory {
 public:
  TestTimeZoneAPILoaderFactory(const GURL& url,
                               const std::string& response,
                               const size_t require_retries)
      : url_(url), response_(response), require_retries_(require_retries) {
    SetInterceptor(base::BindRepeating(&TestTimeZoneAPILoaderFactory::Intercept,
                                       base::Unretained(this)));
    AddResponseWithCode(net::HTTP_INTERNAL_SERVER_ERROR);
  }

  void Intercept(const network::ResourceRequest& request) {
    EXPECT_EQ(url_, request.url);

    EXPECT_NE(nullptr, provider_);
    EXPECT_EQ(provider_->requests_.size(), 1U);

    TimeZoneRequest* timezone_request = provider_->requests_[0].get();

    const base::TimeDelta base_retry_interval =
        base::TimeDelta::FromMilliseconds(kRequestRetryIntervalMilliSeconds);
    timezone_request->set_retry_sleep_on_server_error_for_testing(
        base_retry_interval);
    timezone_request->set_retry_sleep_on_bad_response_for_testing(
        base_retry_interval);

    if (++attempts_ > require_retries_)
      AddResponseWithCode(net::OK);
  }

  void SetTimeZoneProvider(TimeZoneProvider* provider) { provider_ = provider; }
  size_t attempts() const { return attempts_; }

 private:
  void AddResponseWithCode(int error_code) {
    network::ResourceResponseHead response_head;
    response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    response_head.headers->AddHeader("Content-Type: application/json");
    // If AddResponse() is called multiple times for the same URL, the last
    // one is the one used so there is no need for ClearResponses().
    AddResponse(url_, response_head, response_,
                network::URLLoaderCompletionStatus(error_code));
  }

  GURL url_;
  std::string response_;
  const size_t require_retries_;
  size_t attempts_ = 0;
  TimeZoneProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(TestTimeZoneAPILoaderFactory);
};

class TimeZoneReceiver {
 public:
  TimeZoneReceiver() : server_error_(false) {}

  void OnRequestDone(std::unique_ptr<TimeZoneResponseData> timezone,
                     bool server_error) {
    timezone_ = std::move(timezone);
    server_error_ = server_error;

    message_loop_runner_->Quit();
  }

  void WaitUntilRequestDone() {
    message_loop_runner_.reset(new base::RunLoop);
    message_loop_runner_->Run();
  }

  const TimeZoneResponseData* timezone() const { return timezone_.get(); }
  bool server_error() const { return server_error_; }

 private:
  std::unique_ptr<TimeZoneResponseData> timezone_;
  bool server_error_;
  std::unique_ptr<base::RunLoop> message_loop_runner_;
};

class TimeZoneTest : public testing::Test {
 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(TimeZoneTest, ResponseOK) {
  const SimpleRequest simple_request;

  TestTimeZoneAPILoaderFactory url_factory(simple_request.url,
                                           simple_request.http_response,
                                           0 /* require_retries */);
  TimeZoneProvider provider(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_factory),
      GURL(kTestTimeZoneProviderUrl));
  url_factory.SetTimeZoneProvider(&provider);

  TimeZoneReceiver receiver;

  provider.RequestTimezone(simple_request.position,
                           base::TimeDelta::FromSeconds(1),
                           base::Bind(&TimeZoneReceiver::OnRequestDone,
                                      base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();

  EXPECT_EQ(simple_request.timezone.ToStringForDebug(),
            receiver.timezone()->ToStringForDebug());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(1U, url_factory.attempts());
}

TEST_F(TimeZoneTest, ResponseOKWithRetries) {
  const SimpleRequest simple_request;
  TestTimeZoneAPILoaderFactory url_factory(simple_request.url,
                                           simple_request.http_response,
                                           3 /* require_retries */);
  TimeZoneProvider provider(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_factory),
      GURL(kTestTimeZoneProviderUrl));
  url_factory.SetTimeZoneProvider(&provider);

  TimeZoneReceiver receiver;

  provider.RequestTimezone(simple_request.position,
                           base::TimeDelta::FromSeconds(1),
                           base::Bind(&TimeZoneReceiver::OnRequestDone,
                                      base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(simple_request.timezone.ToStringForDebug(),
            receiver.timezone()->ToStringForDebug());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(4U, url_factory.attempts());
}

TEST_F(TimeZoneTest, InvalidResponse) {
  const SimpleRequest simple_request;

  TestTimeZoneAPILoaderFactory url_factory(
      simple_request.url, "invalid JSON string", 0 /* require_retries */);
  TimeZoneProvider provider(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_factory),
      GURL(kTestTimeZoneProviderUrl));
  url_factory.SetTimeZoneProvider(&provider);

  TimeZoneReceiver receiver;

  const int timeout_seconds = 1;
  size_t expected_retries = static_cast<size_t>(
      timeout_seconds * 1000 / kRequestRetryIntervalMilliSeconds);
  ASSERT_GE(expected_retries, 2U);

  provider.RequestTimezone(simple_request.position,
                           base::TimeDelta::FromSeconds(timeout_seconds),
                           base::Bind(&TimeZoneReceiver::OnRequestDone,
                                      base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(
      "dstOffset=0.000000, rawOffset=0.000000, timeZoneId='', timeZoneName='', "
      "error_message='TimeZone provider at 'https://localhost/' : JSONReader "
      "failed: Line: 1, column: 1, Unexpected token..', status=6 "
      "(REQUEST_ERROR)",
      receiver.timezone()->ToStringForDebug());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_GE(url_factory.attempts(), 2U);
  if (url_factory.attempts() > expected_retries + 1) {
    LOG(WARNING) << "TimeZoneTest::InvalidResponse: Too many attempts ("
                 << url_factory.attempts() << "), no more than "
                 << expected_retries + 1 << " expected.";
  }
  if (url_factory.attempts() < expected_retries - 1) {
    LOG(WARNING) << "TimeZoneTest::InvalidResponse: Too less attempts ("
                 << url_factory.attempts() << "), greater than "
                 << expected_retries - 1 << " expected.";
  }
}

TEST(TimeZoneResolverTest, CheckIntervals) {
  for (int requests_count = 1; requests_count < 10; ++requests_count) {
    EXPECT_EQ(requests_count,
              TimeZoneResolver::MaxRequestsCountForIntervalForTesting(
                  TimeZoneResolver::IntervalForNextRequestForTesting(
                      requests_count)));
  }
}

}  // namespace chromeos
