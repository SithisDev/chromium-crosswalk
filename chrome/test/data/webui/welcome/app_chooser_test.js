// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_app_chooser', function() {
  suite('AppChooserTest', function() {
    const apps = [
      {
        id: 0,
        name: 'First',
        icon: 'first',
        url: 'http://first.example.com',
      },
      {
        id: 1,
        name: 'Second',
        icon: 'second',
        url: 'http://second.example.com',
      },
      {
        id: 2,
        name: 'Third',
        icon: 'third',
        url: 'http://third.example.com',
      },
      {
        id: 3,
        name: 'Fourth',
        icon: 'fourth',
        url: 'http://fourth.example.com',
      },
      {
        id: 4,
        name: 'Fifth',
        icon: 'fifth',
        url: 'http://fifth.example.com',
      },
    ];

    /** @type {welcome.NuxAppProxy} */
    let testAppBrowserProxy;

    /** @type {welcome.ModuleMetricsProxy} */
    let testAppMetricsProxy;

    /** @type {welcome.BookmarkProxy} */
    let testBookmarkBrowserProxy;

    /** @type {AppChooserElement} */
    let testElement;

    setup(async function() {
      testAppBrowserProxy = new TestGoogleAppProxy();
      testAppMetricsProxy = new TestMetricsProxy();
      testBookmarkBrowserProxy = new TestBookmarkProxy();

      welcome.GoogleAppProxyImpl.instance_ = testAppBrowserProxy;
      welcome.GoogleAppsMetricsProxyImpl.instance_ = testAppMetricsProxy;
      welcome.BookmarkProxyImpl.instance_ = testBookmarkBrowserProxy;
      welcome.BookmarkBarManager.instance_ = new welcome.BookmarkBarManager();

      testAppBrowserProxy.setAppList(apps);

      PolymerTest.clearBody();
      testElement = document.createElement('nux-google-apps');
      document.body.appendChild(testElement);
      // Simulate nux-app's onRouteEnter call.
      testElement.onRouteEnter();
      await testAppMetricsProxy.whenCalled('recordPageShown');
      await testAppBrowserProxy.whenCalled('getAppList');
    });

    teardown(function() {
      testElement.remove();
    });

    function getSelected() {
      return Array.from(
          testElement.shadowRoot.querySelectorAll('.option[active]'));
    }

    test('test app chooser options', async function() {
      const options =
          Array.from(testElement.shadowRoot.querySelectorAll('.option'));
      assertEquals(5, options.length);

      // First three options are selected and action button should be enabled.
      assertDeepEquals(options.slice(0, 3), getSelected());
      assertFalse(testElement.$$('.action-button').disabled);

      // Click the first option to deselect it.
      testBookmarkBrowserProxy.reset();
      options[0].click();

      assertEquals(
          1, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
      assertDeepEquals(options.slice(1, 3), getSelected());
      assertFalse(testElement.$$('.action-button').disabled);

      // Click fourth option to select it.
      testBookmarkBrowserProxy.reset();
      options[3].click();

      assertDeepEquals(
          {
            title: apps[3].name,
            url: apps[3].url,
            parentId: '1',
          },
          await testBookmarkBrowserProxy.whenCalled('addBookmark'));

      assertDeepEquals(options.slice(1, 4), getSelected());
      assertFalse(testElement.$$('.action-button').disabled);

      // Click fourth option again to deselect it.
      testBookmarkBrowserProxy.reset();
      options[3].click();

      assertEquals(
          4, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
      assertDeepEquals(options.slice(1, 3), getSelected());
      assertFalse(testElement.$$('.action-button').disabled);

      // Click second option to deselect it.
      testBookmarkBrowserProxy.reset();
      options[1].click();

      assertEquals(
          2, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
      assertDeepEquals(options.slice(2, 3), getSelected());
      assertFalse(testElement.$$('.action-button').disabled);

      // Click third option to deselect all options.
      testBookmarkBrowserProxy.reset();
      options[2].click();

      assertEquals(
          3, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
      assertEquals(0, getSelected().length);
      assertTrue(testElement.$$('.action-button').disabled);
    });

    test('test app chooser skip button', async function() {
      const options = testElement.shadowRoot.querySelectorAll('.option');
      testElement.wasBookmarkBarShownOnInit_ = true;

      // First option should be selected and action button should be enabled.
      testElement.$.noThanksButton.click();
      assertEquals(
          1, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
      assertEquals(
          true, await testBookmarkBrowserProxy.whenCalled('toggleBookmarkBar'));
      await testAppMetricsProxy.whenCalled('recordDidNothingAndChoseSkip');
    });

    test('test app chooser next button', async function() {
      const options = testElement.shadowRoot.querySelectorAll('.option');
      testElement.wasBookmarkBarShownOnInit_ = true;

      // First option should be selected and action button should be enabled.
      testElement.$$('.action-button').click();

      await testAppMetricsProxy.whenCalled('recordDidNothingAndChoseNext');

      // Test framework only records first result, but should be called 3 times.
      assertEquals(
          0, await testAppBrowserProxy.whenCalled('recordProviderSelected'));
      assertEquals(3, testAppBrowserProxy.providerSelectedCount);
    });
  });
});
