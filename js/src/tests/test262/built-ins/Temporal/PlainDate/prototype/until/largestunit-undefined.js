// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Fallback value for largestUnit option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainDate(2000, 5, 2);
const later = new Temporal.PlainDate(2001, 6, 3);

const explicit = earlier.until(later, { largestUnit: undefined });
TemporalHelpers.assertDuration(explicit, 0, 0, 0, 397, 0, 0, 0, 0, 0, 0, "default largestUnit is day");
const implicit = earlier.until(later, {});
TemporalHelpers.assertDuration(implicit, 0, 0, 0, 397, 0, 0, 0, 0, 0, 0, "default largestUnit is day");

reportCompare(0, 0);
