// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth
description: PlainYearMonth constructor works
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const ym = new Temporal.PlainYearMonth(1976, 11);
assert.sameValue(typeof ym, "object");
TemporalHelpers.assertPlainYearMonth(ym, 1976, 11, "M11");

reportCompare(0, 0);
