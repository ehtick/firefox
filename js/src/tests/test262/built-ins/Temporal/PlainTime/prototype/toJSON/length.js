// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tojson
description: Temporal.PlainTime.prototype.toJSON.length is 0
info: |
    Every built-in function object, including constructors, has a "length" property whose value is
    an integer. Unless otherwise specified, this value is equal to the largest number of named
    arguments shown in the subclause headings for the function description. Optional parameters
    (which are indicated with brackets: [ ]) or rest parameters (which are shown using the form
    «...name») are not included in the default argument count.

    Unless otherwise specified, the "length" property of a built-in function object has the
    attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.PlainTime.prototype.toJSON, "length", {
  value: 0,
  writable: false,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
