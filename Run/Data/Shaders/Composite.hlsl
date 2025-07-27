//------------------------------------------------------------------------------------------------
Texture2D sceneTexture : register(t0);   // Main scene render target
Texture2D godRaysTexture : register(t1); // God rays texture

SamplerState samplerState : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
};

struct v2p_t
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

v2p_t VertexMain(VS_INPUT input)
{
    v2p_t output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    return output;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    // Sample both textures
    float4 sceneColor = sceneTexture.Sample(samplerState, input.uv);
    float4 godRayColor = godRaysTexture.Sample(samplerState, input.uv);

    // Ensure god rays don’t oversaturate colors
    float godRayIntensity = 0.6f; // Adjust this for overall strength
    godRayColor.rgb *= godRayIntensity;

    // Blend god rays with the scene instead of pure addition
    float3 blendedColor = lerp(sceneColor.rgb, godRayColor.rgb + sceneColor.rgb, godRayColor.a * 0.8f);

    // Preserve contrast and prevent over-brightening
    float luminance = dot(blendedColor, float3(0.3, 0.59, 0.11)); // Approximate luminance
    blendedColor = lerp(blendedColor, blendedColor * 0.8f, saturate(luminance - 1.0f));

    return float4(blendedColor, 1.0);
}
