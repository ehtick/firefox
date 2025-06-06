// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-suppressederror-prototype-objects
description: The prototype of SuppressedError.prototype constructor is Error.prototype
info: |
  Properties of the SuppressedError Prototype Object

  - has a [[Prototype]] internal slot whose value is the intrinsic object %Error.prototype%.
features: [explicit-resource-management]
---*/

var proto = Object.getPrototypeOf(SuppressedError.prototype);

assert.sameValue(proto, Error.prototype);

reportCompare(0, 0);
