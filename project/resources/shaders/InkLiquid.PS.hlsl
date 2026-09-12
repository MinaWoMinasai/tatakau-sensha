struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation float4 color : COLOR0;
    nointerpolation float4 parameters : TEXCOORD1;
    nointerpolation float3 side : TEXCOORD2;
    nointerpolation float3 along : TEXCOORD3;
    nointerpolation float3 facing : TEXCOORD4;
};
struct PixelOutput
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 material : SV_Target2;
};

PixelOutput main(VertexOutput input)
{
    const float kind = input.parameters.y;
    const float age = input.parameters.x;
    const float fade = input.parameters.w > 0.5f ? 1.0f - smoothstep(0.25f, 1.0f, age) : 1.0f;
    float coverage;
    float3 normal = input.facing;
    float3 pigment;
    if (kind > 3.5f)
    {
        const float radial = length(input.uv);
        const float halfWidth = input.parameters.z * 0.5f;
        const float distance = abs(radial - (0.93f - halfWidth));
        const float antialias = max(fwidth(radial), 0.002f);
        coverage = 1.0f - smoothstep(halfWidth - antialias, halfWidth + antialias, distance);
        pigment = input.color.rgb * 0.76f + float3(0.014f, 0.022f, 0.021f) *
            pow(saturate(0.5f + input.uv.y * 0.5f), 6.0f);
        coverage *= 0.65f;
    }
    else
    {
        float2 q = input.uv;
        if (kind < 1.5f)
        {
            // Fat leading head and narrower trailing end; drops retain a
            // slightly rounder body than the main liquid projectile.
            const float taper = kind < 0.5f ? 0.24f : 0.15f;
            q.x /= max(0.38f, 0.74f + q.y * taper);
            q.y = (q.y - 0.045f) * 1.065f;
        }
        else
        {
            // Short cosmetic spray lobes for impact and muzzle liquid bursts.
            // This shape is never used to modify the gameplay paint footprint.
            const float angle = atan2(q.y, q.x);
            const float wobble = 0.85f + 0.065f * sin(angle * 6.0f + age * 2.0f) +
                0.035f * sin(angle * 9.0f - 0.4f);
            q /= wobble;
        }
        const float radialSquared = dot(q, q);
        const float antialias = max(fwidth(radialSquared), 0.006f);
        coverage = 1.0f - smoothstep(1.0f - antialias, 1.0f + antialias, radialSquared);
        const float height = sqrt(saturate(1.0f - radialSquared));
        normal = normalize(input.side * q.x + input.along * q.y + input.facing * height);
        const float3 light = normalize(float3(-0.38f, 0.88f, -0.30f));
        const float lightAmount = 0.78f + 0.24f * saturate(dot(normal, light));
        // Dense colored center and softer rim read as pigment in liquid. The
        // tiny highlight is bounded and nonemissive; blending is ordinary alpha.
        pigment = input.color.rgb * lerp(0.92f, 0.68f, height) * lightAmount;
        const float2 highlightPosition = (q - float2(-0.27f, 0.34f)) / float2(0.16f, 0.23f);
        const float highlight = exp(-dot(highlightPosition, highlightPosition) * 2.3f) * 0.12f;
        pigment += float3(0.74f, 0.88f, 0.86f) * highlight;
        coverage *= lerp(0.88f, 1.0f, height);
    }
    const float alpha = saturate(coverage * input.color.a * fade);
    clip(alpha - 0.008f);
    PixelOutput output;
    output.color = float4(saturate(pigment), alpha);
    // Valid scene layout; PSO keeps these targets unchanged because this
    // transparent layer does not replace the opaque depth used by SSAO/SSR.
    output.normal = float4(normal * 0.5f + 0.5f, 0.0f);
    output.material = float4(0.32f, 0.0f, 1.0f, 0.35f);
    return output;
}
