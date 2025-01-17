// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/chrome_colors/chrome_colors_service.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/search/generated_colors_info.h"
#include "chrome/common/search/selected_colors_info.h"

namespace {

// Records whether current theme changes are confirmed or reverted.
void RecordChangesConfirmedHistogram(bool confirmed) {
  UMA_HISTOGRAM_BOOLEAN("ChromeColors.ChangesConfirmed", confirmed);
}

}  // namespace

namespace chrome_colors {

ChromeColorsService::ChromeColorsService(Profile* profile)
    : theme_service_(ThemeServiceFactory::GetForProfile(profile)) {
  // Determine if we are using a third-party NTP. When user switches to
  // third-party NTP we should revert all the changes.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service) {
    search_provider_observer_ = std::make_unique<SearchProviderObserver>(
        template_url_service,
        base::BindRepeating(&ChromeColorsService::OnSearchProviderChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

ChromeColorsService::~ChromeColorsService() = default;

// static
int ChromeColorsService::GetColorId(const SkColor color) {
  for (chrome_colors::ColorInfo color_info :
       chrome_colors::kGeneratedColorsInfo) {
    if (color == color_info.color)
      return color_info.id;
  }

  return 0;
}

// static
void ChromeColorsService::RecordColorOnLoadHistogram(SkColor color) {
  UMA_HISTOGRAM_ENUMERATION("ChromeColors.ColorOnLoad", GetColorId(color),
                            kNumColorsInfo);
}

void ChromeColorsService::ApplyDefaultTheme(content::WebContents* tab) {
  if (!search_provider_observer_ || !search_provider_observer_->is_google())
    return;
  SaveThemeRevertState(tab);
  theme_service_->UseDefaultTheme();
}

void ChromeColorsService::ApplyAutogeneratedTheme(SkColor color,
                                                  content::WebContents* tab) {
  if (!search_provider_observer_ || !search_provider_observer_->is_google())
    return;
  SaveThemeRevertState(tab);
  theme_service_->BuildFromColor(color);
}

void ChromeColorsService::RevertThemeChangesForTab(content::WebContents* tab) {
  if (!search_provider_observer_ || !search_provider_observer_->is_google() ||
      dialog_tab_ != tab)
    return;
  RevertThemeChangesWithReason(RevertReason::TAB_CLOSED);
}

void ChromeColorsService::RevertThemeChanges() {
  if (!search_provider_observer_ || !search_provider_observer_->is_google())
    return;
  RevertThemeChangesWithReason(RevertReason::MENU_CANCEL);
}

void ChromeColorsService::ConfirmThemeChanges() {
  if (!search_provider_observer_ || !search_provider_observer_->is_google())
    return;
  revert_theme_changes_.Reset();
  dialog_tab_ = nullptr;
  RecordChangesConfirmedHistogram(true);
}

void ChromeColorsService::RevertThemeChangesWithReason(RevertReason reason) {
  if (!revert_theme_changes_.is_null()) {
    std::move(revert_theme_changes_).Run();
    revert_theme_changes_.Reset();
    dialog_tab_ = nullptr;
    UMA_HISTOGRAM_ENUMERATION("ChromeColors.RevertReason", reason);
    RecordChangesConfirmedHistogram(false);
  }
}

void ChromeColorsService::OnSearchProviderChanged() {
  if (search_provider_observer_ && !search_provider_observer_->is_google())
    RevertThemeChangesWithReason(RevertReason::SEARCH_PROVIDER_CHANGE);
}

void ChromeColorsService::SaveThemeRevertState(content::WebContents* tab) {
  // TODO(crbug.com/980745): Support theme reverting for multiple tabs.
  if (revert_theme_changes_.is_null()) {
    revert_theme_changes_ = theme_service_->GetRevertThemeCallback();
    dialog_tab_ = tab;
  }
}

void ChromeColorsService::Shutdown() {}

}  // namespace chrome_colors
