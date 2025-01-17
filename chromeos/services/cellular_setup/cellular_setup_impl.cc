// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/cellular_setup_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/cellular_setup/ota_activator_impl.h"

namespace chromeos {

namespace cellular_setup {

namespace {

CellularSetupImpl::Factory* g_test_factory = nullptr;

}  // namespace

// static
std::unique_ptr<CellularSetupBase> CellularSetupImpl::Factory::Create() {
  if (g_test_factory)
    return g_test_factory->BuildInstance();

  return base::WrapUnique(new CellularSetupImpl());
}

// static
void CellularSetupImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  g_test_factory = test_factory;
}

CellularSetupImpl::Factory::~Factory() = default;

CellularSetupImpl::CellularSetupImpl() = default;

CellularSetupImpl::~CellularSetupImpl() = default;

void CellularSetupImpl::StartActivation(
    mojom::ActivationDelegatePtr delegate,
    StartActivationCallback callback) {
  size_t request_id = next_request_id_;
  ++next_request_id_;

  NetworkHandler* network_handler = NetworkHandler::Get();
  std::unique_ptr<OtaActivator> ota_activator =
      OtaActivatorImpl::Factory::Create(
          std::move(delegate),
          base::BindOnce(&CellularSetupImpl::OnActivationAttemptFinished,
                         base::Unretained(this), request_id),
          network_handler->network_state_handler(),
          network_handler->network_connection_handler(),
          network_handler->network_activation_handler());

  std::move(callback).Run(ota_activator->GenerateInterfacePtr());

  // Store the OtaActivator instance in a map indexed by request ID; once the
  // attempt has finished, the map entry will be deleted in
  // OnActivationAttemptFinished().
  ota_activator_map_.AddWithID(std::move(ota_activator), request_id);
}

void CellularSetupImpl::OnActivationAttemptFinished(size_t request_id) {
  ota_activator_map_.Remove(request_id);
}

}  // namespace cellular_setup

}  // namespace chromeos
