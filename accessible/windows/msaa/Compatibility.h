/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMPATIBILITY_MANAGER_H
#define COMPATIBILITY_MANAGER_H

#include <windows.h>
#include "nsTArray.h"
#include "nsString.h"

#include <stdint.h>

namespace mozilla::a11y {

enum class SuppressionReasons : uint8_t {
  None = 0,
  Clipboard = 1 << 0,
  SnapLayouts = 1 << 1,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SuppressionReasons);

/**
 * Used to get compatibility modes. Note, modes are computed at accessibility
 * start up time and aren't changed during lifetime.
 */
class Compatibility {
 public:
  /**
   * Return true if JAWS mode is enabled.
   */
  static bool IsJAWS() { return !!(sConsumers & (JAWS | OLDJAWS)); }

  /**
   * Return true if using an e10s incompatible Jaws.
   */
  static bool IsOldJAWS() { return !!(sConsumers & OLDJAWS); }

  /**
   * Return true if WE mode is enabled.
   */
  static bool IsWE() { return !!(sConsumers & WE); }

  /**
   * Return true if Dolphin mode is enabled.
   */
  static bool IsDolphin() { return !!(sConsumers & DOLPHIN); }

  /**
   * Return true if JAWS, ZoomText or ZoomText Fusion 2021 or later is being
   * used. These products share common code for interacting with Firefox and
   * all require window emulation to be enabled.
   */
  static bool IsVisperoShared() { return !!(sConsumers & VISPEROSHARED); }

  /*
   * Returns true if the instantiator is a known screen reader.
   */
  static bool IsKnownScreenReader() {
    return IsJAWS() || IsDolphin() || IsVisperoShared() ||
           !!(sConsumers & NVDA);
  }

  /**
   * Return a string describing sConsumers suitable for about:support.
   * Exposed through nsIXULRuntime.accessibilityInstantiator.
   */
  static void GetHumanReadableConsumersStr(nsAString& aResult);

  /**
   * Initialize compatibility mode information.
   */
  static void Init();

  static void GetUiaClientPids(nsTArray<DWORD>& aPids);

  /**
   * return true if a known, non-UIA a11y consumer is present
   */
  static bool HasKnownNonUiaConsumer();

  /**
   * Return true if a module's version is lesser than the given version.
   * Generally, the version should be provided using the MAKE_FILE_VERSION
   * macro.
   * If the version information cannot be retrieved, true is returned; i.e.
   * no version information implies an earlier version.
   */
  static bool IsModuleVersionLessThan(HMODULE aModuleHandle,
                                      unsigned long long aVersion);

  static void SuppressA11yForClipboardCopy();
  static void SuppressA11yForSnapLayouts();
  static bool IsA11ySuppressed() {
    return A11ySuppressionReasons() != SuppressionReasons::None;
  }
  static SuppressionReasons A11ySuppressionReasons();

  /**
   * Returns true if Gecko's native UI Automation implementation is enabled.
   * This is primarily configured via the accessibility.uia.enable pref.
   * However, it might be disabled if Gecko detects known incompatible clients
   * which would otherwise break.
   */
  static bool IsUiaEnabled();

 private:
  Compatibility();
  Compatibility(const Compatibility&);
  Compatibility& operator=(const Compatibility&);

  static void InitConsumers();

  /**
   * List of detected consumers of a11y (used for statistics/telemetry and
   * compat)
   */
  enum {
    NVDA = 1 << 0,
    JAWS = 1 << 1,
    OLDJAWS = 1 << 2,
    WE = 1 << 3,
    DOLPHIN = 1 << 4,
    SEROTEK = 1 << 5,
    COBRA = 1 << 6,
    ZOOMTEXT = 1 << 7,
    KAZAGURU = 1 << 8,
    YOUDAO = 1 << 9,
    UNKNOWN = 1 << 10,
    UIAUTOMATION = 1 << 11,
    VISPEROSHARED = 1 << 12
  };
#define CONSUMERS_ENUM_LEN 13

 private:
  static uint32_t sConsumers;
};

}  // namespace mozilla::a11y

// Convert the 4 (decimal) components of a DLL version number into a
// single unsigned long long, as needed by
// mozilla::a11y::Compatibility::IsModuleVersionLessThan.
#define MAKE_FILE_VERSION(a, b, c, d) \
  ((a##ULL << 48) + (b##ULL << 32) + (c##ULL << 16) + d##ULL)

#endif
