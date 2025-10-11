
float4x4 g_matWorldViewProj;
float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

// ============================
// 立方体描画用
// ============================
void VS_Default(in float4 inPosition : POSITION,
                in float4 inNormal : NORMAL0,
                in float4 inTexCood : TEXCOORD0,
                out float4 outPosition : POSITION,
                out float4 outDiffuse : COLOR0,
                out float4 outTexCood : TEXCOORD0)
{
    outPosition = mul(inPosition, g_matWorldViewProj);
    float lightIntensity = dot(inNormal, g_lightNormal);
    outDiffuse.rgb = max(0, lightIntensity);
    outDiffuse.a = 1.0f;
    outTexCood = inTexCood;
}

void PS_Default(in float4 inScreenColor : COLOR0,
                in float2 inTexCood : TEXCOORD0,
                out float4 outColor : COLOR)
{
    float4 texColor = tex2D(textureSampler, inTexCood);
    outColor = inScreenColor * texColor;
}

technique Technique1
{
    pass P0
    {
        VertexShader = compile vs_2_0 VS_Default();
        PixelShader = compile ps_2_0 PS_Default();
    }
}

// ============================
// ポストエフェクト（201×201, tap=25, step=8）
// サンプル: 0, ±8, ±16, …, ±96
// σ=40, 離散和(間引き)で正規化済み（1D和=1）
// ============================
float2 g_TexelSize;
texture g_SrcTex;
sampler SrcSampler = sampler_state
{
    Texture = <g_SrcTex>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

// ---- 横方向 ----
float4 GaussianSparseH(float2 texCoord : TEXCOORD0) : COLOR
{
    float2 step = float2(g_TexelSize.x, 0.0);
    float4 c = tex2D(SrcSampler, texCoord) / 25;

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        c += tex2D(SrcSampler, texCoord + step * i) / 25;
        c += tex2D(SrcSampler, texCoord - step * i) / 25;
    }
    return c;
}

// ---- 縦方向 ----
float4 GaussianSparseV(float2 texCoord : TEXCOORD0) : COLOR
{
    float2 step = float2(0.0, g_TexelSize.y);
    float4 c = tex2D(SrcSampler, texCoord) / 25;

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        c += tex2D(SrcSampler, texCoord + step * i) / 25;
        c += tex2D(SrcSampler, texCoord - step * i) / 25;
    }
    return c;
}

technique GaussianH
{
    pass P0
    {
        PixelShader = compile ps_3_0 GaussianSparseH();
    }
}
technique GaussianV
{
    pass P0
    {
        PixelShader = compile ps_3_0 GaussianSparseV();
    }
}

