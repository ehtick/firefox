/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var expect = true;
var actual;

var checkCaller = function(me) {
    var f = me['doThing'];
    delete MyObj.prototype['doThing'];
    var caller = arguments.callee.caller;
    var callerIsMethod = (f === caller);
    actual = callerIsMethod;
};

var MyObj = function() {
};

MyObj.prototype.doThing = function() {
    checkCaller(this);
};

(new MyObj()).doThing();

assert.sameValue(expect, actual, "ok");

reportCompare(0, 0);
