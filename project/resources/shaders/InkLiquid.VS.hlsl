struct Particle
{
    float4 centerRadius;
    float4 axisStretch;
    float4 color;
    float4 parameters;
};
cbuffer Frame : register(b0)
{
    row_major float4x4 viewProjection;
    float4 eye;
    float4 cameraRight;
    float4 cameraUp;
};
StructuredBuffer<Particle> particles : register(t0);

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

VertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        float2(-1, 1), float2(1, -1), float2(1, 1)
    };
    const Particle particle = particles[instanceId];
    VertexOutput output;
    output.uv = corners[vertexId];
    output.color = particle.color;
    output.parameters = particle.parameters;
    float3 side;
    float3 along;
    float3 facing;
    float stretch = 1.0f;
    if (particle.parameters.y > 3.5f)
    {
        // Rings lie on the actual impact/ink surface, including vertical walls.
        facing = normalize(particle.axisStretch.xyz);
        const float3 reference = abs(facing.y) < 0.9f ? float3(0, 1, 0) : float3(1, 0, 0);
        side = normalize(cross(reference, facing));
        along = normalize(cross(facing, side));
    }
    else
    {
        const float2 projectedVelocity = float2(dot(particle.axisStretch.xyz, cameraRight.xyz),
            dot(particle.axisStretch.xyz, cameraUp.xyz));
        const float projectedSpeed = length(projectedVelocity);
        const float2 direction = projectedSpeed > 0.0001f ? projectedVelocity / projectedSpeed : float2(0, 1);
        side = cameraRight.xyz * direction.y - cameraUp.xyz * direction.x;
        along = cameraRight.xyz * direction.x + cameraUp.xyz * direction.y;
        const float3 toEye = eye.xyz - particle.centerRadius.xyz;
        facing = dot(toEye, toEye) > 0.000001f ? normalize(toEye) : -normalize(cross(cameraRight.xyz, cameraUp.xyz));
        // Stretch follows projected speed. Head-on projectiles stay round; the
        // visible broadside takes a short fluid shape, never a long laser line.
        stretch = particle.axisStretch.w * (1.0f + min(0.75f, projectedSpeed * 0.012f));
    }
    const float3 worldPosition = particle.centerRadius.xyz + particle.centerRadius.w *
        (side * output.uv.x + along * output.uv.y * stretch);
    output.position = mul(float4(worldPosition, 1.0f), viewProjection);
    output.side = side;
    output.along = along;
    output.facing = facing;
    return output;
}
