
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
        VertexShader = compile vs_3_0 VS_Default();
        PixelShader = compile ps_3_0 PS_Default();
    }
}

// ============================
// ポストエフェクト（201×201, tap=25, step=8）
// サンプル: 0, ±8, ±16, …, ±96
// σ=40, 離散和(間引き)で正規化済み（1D和=1）
// ============================
float2 g_TexelSize;
float g_FilterSpacing = 1.0f;
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
    float4 c = tex2D(SrcSampler, texCoord) /25;

    [unroll]
    for (int i = 1; i <= 12; i++)
    {
        float ofs = i;
        c += tex2D(SrcSampler, texCoord + step * ofs) / 25;
        c += tex2D(SrcSampler, texCoord - step * ofs) / 25;
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
        float ofs = i;
        c += tex2D(SrcSampler, texCoord + step * ofs) / 25;
        c += tex2D(SrcSampler, texCoord - step * ofs) / 25;
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

// ============================
// 3x3 Gaussian / Tent filters for multi-res chain
// ============================

texture g_SrcTex2;
sampler SrcSampler2 = sampler_state
{
    Texture   = <g_SrcTex2>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU  = CLAMP;
    AddressV  = CLAMP;
};

float GaussianWeight11(int offset)
{
    int a = abs(offset);
    if (a == 0) { return 252.0; }
    if (a == 1) { return 210.0; }
    if (a == 2) { return 120.0; }
    if (a == 3) { return 45.0; }
    if (a == 4) { return 10.0; }
    return 1.0;
}

float4 Gaussian11x11(sampler srcSampler, float2 uv)
{
    float2 ts = g_TexelSize * g_FilterSpacing;
    float4 sum = 0.0;

    [unroll]
    for (int y = -5; y <= 5; ++y)
    {
        float wy = GaussianWeight11(y);

        [unroll]
        for (int x = -5; x <= 5; ++x)
        {
            float wx = GaussianWeight11(x);
            sum += tex2D(srcSampler, uv + float2((float)x * ts.x, (float)y * ts.y)) * (wx * wy);
        }
    }

    return sum / 1048576.0;
}

// 11x11 Gaussian 相当（binomial 10th row の2D積）を正規化
float4 PS_Down11x11(float2 uv : TEXCOORD0) : COLOR
{
    return Gaussian11x11(SrcSampler, uv);
}

// 低レベルを3x3で広げつつアップサンプルし、ひとつ上のレベルを加算
float4 PS_UpsampleAdd11x11(float2 uv : TEXCOORD0) : COLOR
{
    float4 low = Gaussian11x11(SrcSampler, uv);
    float4 hi  = tex2D(SrcSampler2, uv);

    return low + hi;
}

// 低レベルだけでアップサンプル（最終段など）
float4 PS_UpsampleOnly11x11(float2 uv : TEXCOORD0) : COLOR
{
    return Gaussian11x11(SrcSampler, uv);
}

// 単純コピー（デバッグ用）
float4 Gaussian3x3(sampler srcSampler, float2 uv)
{
    float2 ts = g_TexelSize * g_FilterSpacing;
    float4 sumCenter = tex2D(srcSampler, uv) * 4.0;

    float4 sumCross = 0.0;
    sumCross += tex2D(srcSampler, uv + float2(+ts.x, 0.0)) * 2.0;
    sumCross += tex2D(srcSampler, uv + float2(-ts.x, 0.0)) * 2.0;
    sumCross += tex2D(srcSampler, uv + float2(0.0, +ts.y)) * 2.0;
    sumCross += tex2D(srcSampler, uv + float2(0.0, -ts.y)) * 2.0;

    float4 sumDiag = 0.0;
    sumDiag += tex2D(srcSampler, uv + ts);
    sumDiag += tex2D(srcSampler, uv + float2(+ts.x, -ts.y));
    sumDiag += tex2D(srcSampler, uv + float2(-ts.x, +ts.y));
    sumDiag += tex2D(srcSampler, uv - ts);

    return (sumCenter + sumCross + sumDiag) / 16.0;
}

float4 PS_Down3x3(float2 uv : TEXCOORD0) : COLOR
{
    return Gaussian3x3(SrcSampler, uv);
}

float4 PS_UpsampleAdd3x3(float2 uv : TEXCOORD0) : COLOR
{
    float4 low = Gaussian3x3(SrcSampler, uv);
    float4 hi = tex2D(SrcSampler2, uv);
    return low + hi;
}

float4 PS_UpsampleOnly3x3(float2 uv : TEXCOORD0) : COLOR
{
    return Gaussian3x3(SrcSampler, uv);
}

texture g_LevelTex0;
texture g_LevelTex1;
texture g_LevelTex2;
texture g_LevelTex3;
texture g_LevelTex4;

sampler LevelSampler0 = sampler_state { Texture = <g_LevelTex0>; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; };
sampler LevelSampler1 = sampler_state { Texture = <g_LevelTex1>; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; };
sampler LevelSampler2 = sampler_state { Texture = <g_LevelTex2>; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; };
sampler LevelSampler3 = sampler_state { Texture = <g_LevelTex3>; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; };
sampler LevelSampler4 = sampler_state { Texture = <g_LevelTex4>; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; };

float4 PS_CompositeLevels(float2 uv : TEXCOORD0) : COLOR
{
    float4 color = 0.0;
    color += tex2D(LevelSampler0, uv);
    color += tex2D(LevelSampler1, uv);
    color += tex2D(LevelSampler2, uv);
    color += tex2D(LevelSampler3, uv);
    color += tex2D(LevelSampler4, uv);
    return color / 5.0;
}

float4 PS_Copy(float2 uv : TEXCOORD0) : COLOR
{
    return tex2D(SrcSampler, uv);
}

technique Down3x3
{
    pass P0
    {
        PixelShader = compile ps_3_0 PS_Down3x3();
    }
}

technique UpsampleAdd3x3
{
    pass P0
    {
        PixelShader = compile ps_3_0 PS_UpsampleAdd3x3();
    }
}

technique UpsampleOnly3x3
{
    pass P0
    {
        PixelShader = compile ps_3_0 PS_UpsampleOnly3x3();
    }
}

technique CompositeLevels
{
    pass P0
    {
        PixelShader = compile ps_3_0 PS_CompositeLevels();
    }
}

technique Copy
{
    pass P0
    {
        PixelShader = compile ps_3_0 PS_Copy();
    }
}

