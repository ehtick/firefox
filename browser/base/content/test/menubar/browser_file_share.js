/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);
const TEST_URL = BASE + "file_shareurl.html";

let mockShareData = [
  {
    name: "Test",
    menuItemTitle: "Sharing Service Test",
    image:
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKE" +
      "lEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==",
  },
];

// Setup spies for observing function calls from MacSharingService
let shareUrlSpy = sinon.spy();
let openSharingPreferencesSpy = sinon.spy();
let getSharingProvidersSpy = sinon.spy();

let { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);
let mockMacSharingService = MockRegistrar.register(
  "@mozilla.org/widget/macsharingservice;1",
  {
    getSharingProviders(url) {
      getSharingProvidersSpy(url);
      return mockShareData;
    },
    shareUrl(name, url, title) {
      shareUrlSpy(name, url, title);
    },
    openSharingPreferences() {
      openSharingPreferencesSpy();
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIMacSharingService]),
  }
);

registerCleanupFunction(function () {
  MockRegistrar.unregister(mockMacSharingService);
});

/**
 * Test the "Share" item menus in the tab contextmenu on MacOSX.
 */
add_task(async function test_file_menu_share() {
  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    // We can't toggle menubar items on OSX, so mocking instead.
    let menu = document.getElementById("menu_FilePopup");
    await simulateMenuOpen(menu);

    await BrowserTestUtils.waitForMutationCondition(
      menu,
      { childList: true },
      () => menu.querySelector(".share-tab-url-item")
    );
    ok(true, "Got Share item");

    let popup = menu.querySelector(".share-tab-url-item").menupopup;
    await simulateMenuOpen(popup);
    ok(getSharingProvidersSpy.calledOnce, "getSharingProviders called");

    info(
      "Check we have copy link, a service and one extra menu item for the More... button"
    );
    let items = Array.from(popup.querySelectorAll("menuitem"));
    is(items.length, 3, "There should be 2 sharing services.");

    info("Click on the sharing service");
    let shareButton = items.find(
      t => t.label == mockShareData[0].menuItemTitle
    );
    is(
      shareButton.label,
      mockShareData[0].menuItemTitle,
      "Share button's label should match the service's menu item title. "
    );
    is(
      shareButton.getAttribute("share-name"),
      mockShareData[0].name,
      "Share button's share-name value should match the service's name. "
    );

    shareButton.doCommand();

    ok(shareUrlSpy.calledOnce, "shareUrl called");

    info("Check the correct data was shared.");
    let [name, url, title] = shareUrlSpy.getCall(0).args;
    is(name, mockShareData[0].name, "Shared correct service name");
    is(url, TEST_URL, "Shared correct URL");
    is(title, "Sharing URL", "Shared the correct title.");
    await simulateMenuClosed(popup);
    await simulateMenuClosed(menu);

    info("Test the copy link button");
    await simulateMenuOpen(menu);
    popup = menu.querySelector(".share-tab-url-item").menupopup;
    await simulateMenuOpen(popup);
    // Since the menu was collapsed previously, the popup needs to get the
    // providers again.
    ok(getSharingProvidersSpy.calledTwice, "getSharingProviders called again");
    items = Array.from(popup.querySelectorAll("menuitem"));
    is(items.length, 3, "There should be 3 sharing services.");
    info("Click on the Copy Link item");
    let copyLinkItem = items.find(
      item => item.dataset.l10nId == "menu-share-copy-link"
    );
    await SimpleTest.promiseClipboardChange(TEST_URL, () =>
      copyLinkItem.doCommand()
    );
    await simulateMenuClosed(popup);
    await simulateMenuClosed(menu);

    info("Test the More... button");

    await simulateMenuOpen(menu);
    popup = menu.querySelector(".share-tab-url-item").menupopup;
    await simulateMenuOpen(popup);
    // Since the menu was collapsed previously, the popup needs to get the
    // providers again.
    is(getSharingProvidersSpy.callCount, 3, "getSharingProviders called again");
    items = popup.querySelectorAll("menuitem");
    is(items.length, 3, "There should be 3 sharing services.");

    info("Click on the More Button");
    let moreButton = items[2];
    moreButton.doCommand();
    ok(openSharingPreferencesSpy.calledOnce, "openSharingPreferences called");
    // Tidy up:
    await simulateMenuClosed(popup);
    await simulateMenuClosed(menu);
  });
});

async function simulateMenuOpen(menu) {
  return new Promise(resolve => {
    menu.addEventListener("popupshown", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popupshowing", { bubbles: true }));
    menu.dispatchEvent(new MouseEvent("popupshown", { bubbles: true }));
  });
}

async function simulateMenuClosed(menu) {
  return new Promise(resolve => {
    menu.addEventListener("popuphidden", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popuphiding", { bubbles: true }));
    menu.dispatchEvent(new MouseEvent("popuphidden", { bubbles: true }));
  });
}
