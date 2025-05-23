/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.bounceTrackingProtection.mode",
        Ci.nsIBounceTrackingProtection.MODE_ENABLED,
      ],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
  bounceTrackingProtection = Cc[
    "@mozilla.org/bounce-tracking-protection;1"
  ].getService(Ci.nsIBounceTrackingProtection);
});

// Tests that the bounce tracking protection feature works correctly when the
// tab is closed before the extended navigation ends.
// The bounce tracker should still be classified and purged correctly.

add_task(async function test_bounce_tab_close() {
  if (
    !Services.prefs.getBoolPref(
      "privacy.bounceTrackingProtection.requireStatefulBounces"
    )
  ) {
    // TODO: Bug 1959947: Make this test work for BTP stateless mode.
    ok(true, "Skipping test because requireStatefulBounces is disabled.");
    return;
  }

  info(
    "Test bounce where extended navigation ends early because of tab close."
  );
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    closeTabAfterBounce: true,
  });
});
