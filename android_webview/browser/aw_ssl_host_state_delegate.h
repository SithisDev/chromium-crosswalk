// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SSL_HOST_STATE_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SSL_HOST_STATE_DELEGATE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

namespace android_webview {

namespace internal {
// This class maintains the policy for storing actions on certificate errors.
class CertPolicy {
 public:
  CertPolicy();
  ~CertPolicy();
  // Returns true if the user has decided to proceed through the ssl error
  // before. For a certificate to be allowed, it must not have any
  // *additional* errors from when it was allowed.
  bool Check(const net::X509Certificate& cert, int error) const;

  // Causes the policy to allow this certificate for a given |error|. And
  // remember the user's choice.
  void Allow(const net::X509Certificate& cert, int error);

  // Returns true if and only if there exists a user allow exception for some
  // certificate.
  bool HasAllowException() const { return allowed_.size() > 0; }

 private:
  // The set of fingerprints of allowed certificates.
  std::map<net::SHA256HashValue, int> allowed_;
};

}  // namespace internal

class AwSSLHostStateDelegate : public content::SSLHostStateDelegate {
 public:
  AwSSLHostStateDelegate();
  ~AwSSLHostStateDelegate() override;

  // Records that |cert| is permitted to be used for |host| in the future, for
  // a specified |error| type.
  void AllowCert(const std::string& host,
                 const net::X509Certificate& cert,
                 int error) override;

  void Clear(
      const base::Callback<bool(const std::string&)>& host_filter) override;

  // Queries whether |cert| is allowed or denied for |host| and |error|.
  content::SSLHostStateDelegate::CertJudgment QueryPolicy(
      const std::string& host,
      const net::X509Certificate& cert,
      int error,
      bool* expired_previous_decision) override;

  // Records that a host has run insecure content.
  void HostRanInsecureContent(const std::string& host,
                              int child_id,
                              InsecureContentType content_type) override;

  // Returns whether the specified host ran insecure content.
  bool DidHostRunInsecureContent(const std::string& host,
                                 int child_id,
                                 InsecureContentType content_type) override;

  // Revokes all SSL certificate error allow exceptions made by the user for
  // |host|.
  void RevokeUserAllowExceptions(const std::string& host) override;

  // Returns whether the user has allowed a certificate error exception for
  // |host|. This does not mean that *all* certificate errors are allowed, just
  // that there exists an exception. To see if a particular certificate and
  // error combination exception is allowed, use QueryPolicy().
  bool HasAllowException(const std::string& host) override;

 private:
  // Certificate policies for each host.
  std::map<std::string, internal::CertPolicy> cert_policy_for_host_;

  DISALLOW_COPY_AND_ASSIGN(AwSSLHostStateDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SSL_HOST_STATE_DELEGATE_H_
