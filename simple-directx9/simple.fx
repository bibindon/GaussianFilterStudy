float4x4 g_matWorldViewProj;
float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MinFilter = LINEAR;
    MagFilter = LINEAR;
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
// ポストエフェクト用
// ============================
float2 g_TexelSize;
texture g_SrcTex;
sampler SrcSampler = sampler_state
{
    Texture = <g_SrcTex>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

float4 CopyPS(float2 texCoord : TEXCOORD0) : COLOR
{
    return tex2D(SrcSampler, texCoord);
}

technique Copy
{
    pass P0
    {
        PixelShader = compile ps_2_0 CopyPS();
    }
}

float4 GaussianBlurH(float2 texCoord : TEXCOORD0) : COLOR
{
    float4 c = 0;
    c += tex2D(SrcSampler, texCoord + float2(-4.0, 0) * g_TexelSize) * 0.05;
    c += tex2D(SrcSampler, texCoord + float2(-2.0, 0) * g_TexelSize) * 0.25;
    c += tex2D(SrcSampler, texCoord) * 0.40;
    c += tex2D(SrcSampler, texCoord + float2(2.0, 0) * g_TexelSize) * 0.25;
    c += tex2D(SrcSampler, texCoord + float2(4.0, 0) * g_TexelSize) * 0.05;
    return c;
}

float4 GaussianBlurV(float2 texCoord : TEXCOORD0) : COLOR
{
    float4 c = 0;
    c += tex2D(SrcSampler, texCoord + float2(0, -4.0) * g_TexelSize) * 0.05;
    c += tex2D(SrcSampler, texCoord + float2(0, -2.0) * g_TexelSize) * 0.25;
    c += tex2D(SrcSampler, texCoord) * 0.40;
    c += tex2D(SrcSampler, texCoord + float2(0, 2.0) * g_TexelSize) * 0.25;
    c += tex2D(SrcSampler, texCoord + float2(0, 4.0) * g_TexelSize) * 0.05;
    return c;
}

technique GaussianH
{
    pass P0
    {
        PixelShader = compile ps_2_0 GaussianBlurH();
    }
}

technique GaussianV
{
    pass P0
    {
        PixelShader = compile ps_2_0 GaussianBlurV();
    }
}

