// const buffer view (cbv)
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorldViewProj;
};
// 顶点着色器 输入
struct VertexIn
{
	float3 PosL : POSITION;
	float4 Color : COLOR;
};
// 顶点着色器 输出
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

// 顶点着色器
VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.PosH = mul(float4(vin.PosL,1.0f),gWorldViewProj);

	vout.Color = vin.Color;

	return vout;
}

//像素着色器
float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}