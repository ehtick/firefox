// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Brage Hogstad, University of Bergen. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.constructor
description: Various invalid ISO string values for calendar
features: [Temporal]
---*/

const invalidStrings = [
  ["", "empty string"],
  ["1997-12-04[u-ca=iso8601]", "ISO string with calendar annotation"],
];

for (const [arg, description] of invalidStrings) {
  assert.throws(
    RangeError,
    () => new Temporal.ZonedDateTime(0n, "UTC", arg),
    `${description} is not a valid calendar ID`
  );
}

reportCompare(0, 0);
