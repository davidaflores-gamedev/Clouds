Texture3D<float> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

cbuffer SliceParams : register(b0)
{
    int sliceIndex;
    int maxSlices;
    float2 padding;
};

[numthreads(16, 16, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height, depth;
    inputTexture.GetDimensions(width, height, depth);
    
    if (id.x >= width || id.y >= height)
        return;
        
    int z = min(sliceIndex, depth - 1);
    float value = inputTexture[int3(id.x, id.y, z)];
    
    outputTexture[id.xy] = float4(value, value, value, 1.0f);
}