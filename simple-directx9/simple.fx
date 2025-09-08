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
// �����̕`��p
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
// �|�X�g�G�t�F�N�g�p
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
};

// ============================
// �����̕`��p
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
// �|�X�g�G�t�F�N�g�p�i201�~201 �͈͂�25tap�j
// ============================
float2 g_TexelSize;
texture g_SrcTex;
sampler SrcSampler = sampler_state
{
    Texture = <g_SrcTex>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

// �T���v���ʒu: 0, �}8, �}16, �c, �}96  �i���S+12�y�A=25tap�j
// ��=40 �Ő��K���ς݂̏d��
static const float w[13] =
{
    0.064759, // 0
    0.063502, // �}8
    0.059815, // �}16
    0.053249, // �}24
    0.044676, // �}32
    0.035170, // �}40
    0.025782, // �}48
    0.017335, // �}56
    0.010285, // �}64
    0.005312, // �}72
    0.002415, // �}80
    0.000976, // �}88
    0.000362 // �}96
};

// ---- ������ ----
float4 GaussianSparseH(float2 texCoord : TEXCOORD0) : COLOR
{
    float2 step = float2(g_TexelSize.x, 0.0);
    float4 c = tex2D(SrcSampler, texCoord) * w[0];

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        float offset = (float) (i * 8); // 8,16,...,96
        c += tex2D(SrcSampler, texCoord + step * offset) * w[i];
        c += tex2D(SrcSampler, texCoord - step * offset) * w[i];
    }
    return c;
}

// ---- �c���� ----
float4 GaussianSparseV(float2 texCoord : TEXCOORD0) : COLOR
{
    float2 step = float2(0.0, g_TexelSize.y);
    float4 c = tex2D(SrcSampler, texCoord) * w[0];

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        float offset = (float) (i * 8);
        c += tex2D(SrcSampler, texCoord + step * offset) * w[i];
        c += tex2D(SrcSampler, texCoord - step * offset) * w[i];
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

