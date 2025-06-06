export const description = `
Tests that depthBiasClamp must be zero in compat mode.
`;

import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import * as vtu from '../../../../api/validation/validation_test_utils.js';
import { CompatibilityTest } from '../../../compatibility_test.js';

export const g = makeTestGroup(CompatibilityTest);

g.test('depthBiasClamp')
  .desc('Tests that depthBiasClamp must be zero in compat mode.')
  .params(u =>
    u //
      .combine('depthBiasClamp', [undefined, 0, 0.1, 1])
      .combine('async', [false, true] as const)
  )
  .fn(t => {
    const { depthBiasClamp, async } = t.params;

    const module = t.device.createShaderModule({
      code: `
        @vertex fn vs() -> @builtin(position) vec4f {
            return vec4f(0);
        }

        @fragment fn fs() -> @location(0) vec4f {
            return vec4f(0);
        }
      `,
    });

    const pipelineDescriptor: GPURenderPipelineDescriptor = {
      layout: 'auto',
      vertex: {
        module,
        entryPoint: 'vs',
      },
      fragment: {
        module,
        entryPoint: 'fs',
        targets: [{ format: 'rgba8unorm' }],
      },
      depthStencil: {
        format: 'depth24plus',
        depthWriteEnabled: true,
        depthCompare: 'always',
        ...(depthBiasClamp !== undefined && { depthBiasClamp }),
      },
    };

    const success = !t.isCompatibility || !depthBiasClamp;
    vtu.doCreateRenderPipelineTest(t, async, success, pipelineDescriptor);
  });
