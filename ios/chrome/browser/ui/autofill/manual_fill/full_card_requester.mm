// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_requester.h"

#include <vector>

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_request_result_delegate_bridge.h"
#include "ios/chrome/browser/ui/payments/full_card_requester.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface ManualFillFullCardRequester ()

// The ios::ChromeBrowserState instance passed to the initializer.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// The WebStateList for this instance. Used to instantiate the child
// coordinators lazily.
@property(nonatomic, readonly) WebStateList* webStateList;

@end

@implementation ManualFillFullCardRequester {
  std::unique_ptr<FullCardRequester> _fullCardRequester;
  // Obj-C delegate to receive the success or failure result, when
  // asking credit card unlocking.
  std::unique_ptr<FullCardRequestResultDelegateBridge> _cardAssistant;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                        webStateList:(WebStateList*)webStateList
                      resultDelegate:
                          (id<FullCardRequestResultDelegateObserving>)delegate {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _webStateList = webStateList;
    _cardAssistant =
        std::make_unique<FullCardRequestResultDelegateBridge>(delegate);
  }
  return self;
}

- (void)requestFullCreditCard:(autofill::CreditCard)card
       withBaseViewController:(UIViewController*)viewController {
  // Payment Request is only enabled in main frame.
  web::WebState* webState = self.webStateList->GetActiveWebState();
  web::WebFrame* mainFrame = webState->GetWebFramesManager()->GetMainWebFrame();
  autofill::AutofillManager* autofillManager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, mainFrame)
          ->autofill_manager();
  DCHECK(autofillManager);
  _fullCardRequester =
      std::make_unique<FullCardRequester>(viewController, self.browserState);
  _fullCardRequester->GetFullCard(card, autofillManager,
                                  _cardAssistant->GetWeakPtr());
  // TODO(crbug.com/845472): closing CVC requester doesn't restore icon bar
  // above keyboard.
}

@end
