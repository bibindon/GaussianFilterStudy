/*
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
// óßï˚ëÃï`âÊóp
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
        VertexShader = compile vs_3_0 VS_Default();
        PixelShader = compile ps_3_0 PS_Default();
    }
}

// ============================
// É|ÉXÉgÉGÉtÉFÉNÉgóp
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
        PixelShader = compile ps_3_0 CopyPS();
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
        PixelShader = compile ps_3_0 GaussianBlurH();
    }
}

technique GaussianV
{
    pass P0
    {
        PixelShader = compile ps_3_0 GaussianBlurV();
    }
}
*/

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
// óßï˚ëÃï`âÊóp
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
// É|ÉXÉgÉGÉtÉFÉNÉgÅi201Å~201, tap=25, step=8Åj
// ÉTÉìÉvÉã: 0, Å}8, Å}16, Åc, Å}96
// É–=40, ó£éUòa(ä‘à¯Ç´)Ç≈ê≥ãKâªçœÇ›Åi1Dòa=1Åj
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

// ê≥ãKâªçœÇ›Åisum = w0 + 2*É∞w[i] = 1Åj
static const float w[13] =
{
    0.0807799342, // 0
    0.0791803843, // Å}8
    0.0745692777, // Å}16
    0.0674730727, // Å}24
    0.0586582714, // Å}32
    0.0489955068, // Å}40
    0.0393198152, // Å}48
    0.0303176059, // Å}56
    0.0224598348, // Å}64
    0.0159862439, // Å}72
    0.0109323753, // Å}80
    0.0071830824, // Å}88
    0.0045345624 // Å}96
};

// ---- â°ï˚å¸ ----
float4 GaussianSparseH(float2 texCoord : TEXCOORD0) : COLOR
{
    float2 step = float2(g_TexelSize.x, 0.0);
    float4 c = tex2D(SrcSampler, texCoord) * w[0];

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        float ofs = (float) (i * 8); // 8,16,...,96
        c += tex2D(SrcSampler, texCoord + step * ofs) * w[i];
        c += tex2D(SrcSampler, texCoord - step * ofs) * w[i];
    }
    return c;
}

// ---- ècï˚å¸ ----
float4 GaussianSparseV(float2 texCoord : TEXCOORD0) : COLOR
{
    float2 step = float2(0.0, g_TexelSize.y);
    float4 c = tex2D(SrcSampler, texCoord) * w[0];

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        float ofs = (float) (i * 8);
        c += tex2D(SrcSampler, texCoord + step * ofs) * w[i];
        c += tex2D(SrcSampler, texCoord - step * ofs) * w[i];
    }
    return c;
}

technique GaussianH
{
    pass P0
    {
        PixelShader = compile ps_2_0 GaussianSparseH();
    }
}
technique GaussianV
{
    pass P0
    {
        PixelShader = compile ps_2_0 GaussianSparseV();
    }
}

