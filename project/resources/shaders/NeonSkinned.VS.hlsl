#include "NeonSkinnedSkinning.hlsli"

NeonSkinnedVertexOutput main(VertexShaderInput input)
{
    return SkinNeonVertex(input);
}
