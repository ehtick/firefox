/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution tests for the 'dpdyCoarse' builtin function

T is f32 or vecN<f32>
fn dpdyCoarse(e:T) ->T
Returns the partial derivative of e with respect to window y coordinates using local differences.
This may result in fewer unique positions that dpdyFine(e).
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../../../gpu_test.js';

import { d } from './derivatives.cache.js';
import { runDerivativeTest } from './derivatives.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

const builtin = 'dpdyCoarse';

g.test('f32').
specURL('https://www.w3.org/TR/WGSL/#derivative-builtin-functions').
params((u) =>
u.
combine('vectorize', [undefined, 2, 3, 4]).
combine('non_uniform_discard', [false, true])
).
fn(async (t) => {
  const cases = await d.get('scalar');
  runDerivativeTest(t, cases, builtin, t.params.non_uniform_discard, t.params.vectorize);
});