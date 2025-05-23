"use strict";

// NB: This uses some ugly hacks to get into the CUI module from elsewhere...
// don't try this at home, kids.
function test() {
  // Customize something to make sure stuff changed:
  CustomizableUI.addWidgetToArea(
    "save-page-button",
    CustomizableUI.AREA_NAVBAR
  );

  let gFuturePlacements =
    CustomizableUI.getTestOnlyInternalProp("gFuturePlacements");
  is(
    gFuturePlacements.size,
    0,
    "All future placements should be dealt with by now."
  );

  let CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
    "CustomizableUIInternal"
  );

  // Force us to have a saved state:
  CustomizableUIInternal.saveState();
  CustomizableUIInternal.loadSavedState();

  CustomizableUIInternal.updateForNewVersion();
  is(gFuturePlacements.size, 0, "No change to future placements initially.");

  // Add our widget to the defaults:
  let testWidgetNew = {
    id: "test-messing-with-default-placements-new-pref",
    label: "Test messing with default placements - pref-based",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    introducedInVersion: "pref",
  };

  let normalizedWidget = CustomizableUIInternal.normalizeWidget(
    testWidgetNew,
    CustomizableUI.SOURCE_BUILTIN
  );
  ok(normalizedWidget, "Widget should be normalizable");
  if (!normalizedWidget) {
    return;
  }
  let gPalette = CustomizableUI.getTestOnlyInternalProp("gPalette");
  gPalette.set(testWidgetNew.id, normalizedWidget);

  // Now adjust default placements for area:
  let navbarArea = CustomizableUI.getTestOnlyInternalProp("gAreas").get(
    CustomizableUI.AREA_NAVBAR
  );
  let navbarPlacements = navbarArea.get("defaultPlacements");
  navbarPlacements.splice(
    navbarPlacements.indexOf("bookmarks-menu-button") + 1,
    0,
    testWidgetNew.id
  );

  let savedPlacements =
    CustomizableUI.getTestOnlyInternalProp("gSavedState").placements[
      CustomizableUI.AREA_NAVBAR
    ];
  // Then call the re-init routine so we re-add the builtin widgets
  CustomizableUIInternal.updateForNewVersion();
  is(gFuturePlacements.size, 1, "Should have 1 more future placement");
  let futureNavbarPlacements = gFuturePlacements.get(
    CustomizableUI.AREA_NAVBAR
  );
  ok(futureNavbarPlacements, "Should have placements for nav-bar");
  if (futureNavbarPlacements) {
    ok(
      futureNavbarPlacements.has(testWidgetNew.id),
      "widget should be in future placements"
    );
  }
  CustomizableUIInternal.placeNewDefaultWidgetsInArea(
    CustomizableUI.AREA_NAVBAR
  );

  let indexInSavedPlacements = savedPlacements.indexOf(testWidgetNew.id);
  info("Saved placements: " + savedPlacements.join(", "));
  isnot(indexInSavedPlacements, -1, "Widget should have been inserted");
  is(
    indexInSavedPlacements,
    savedPlacements.indexOf("bookmarks-menu-button") + 1,
    "Widget should be in the right place."
  );

  if (futureNavbarPlacements) {
    ok(
      !futureNavbarPlacements.has(testWidgetNew.id),
      "widget should be out of future placements"
    );
  }

  if (indexInSavedPlacements != -1) {
    savedPlacements.splice(indexInSavedPlacements, 1);
  }

  gFuturePlacements.delete(CustomizableUI.AREA_NAVBAR);
  let indexInDefaultPlacements = navbarPlacements.indexOf(testWidgetNew.id);
  if (indexInDefaultPlacements != -1) {
    navbarPlacements.splice(indexInDefaultPlacements, 1);
  }
  gPalette.delete(testWidgetNew.id);
  CustomizableUI.reset();
}
