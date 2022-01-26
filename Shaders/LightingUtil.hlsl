#define MaxLights 16

// 如此设计为了内存布局3 1 = 4
struct Light
{
	float3	Strength; //光源颜色
	float	FalloffStart;	// 衰减值起始 点光源和聚光灯
	float3	Direction; // 方向 平行光和聚光灯
	float	FalloffEnd; // 衰减值终点 点光源和聚光灯
	float3	Position; // 光源位置 点光源和聚光灯
	float	SpotPower; // 聚光灯范围
};

struct Material
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Shininess;
};

// 实现一种线性衰减因子的计算方法,可用于点光源和聚光灯
float CalcAttenuation(float d, float falloffstart, float falloffend)
{
	return saturate((falloffend - d) / (falloffend - falloffstart));
}

// 代替菲涅尔方程的石里克近似
float3 SchlickFresnel(float R0, float3 normal, float3 lightVec)
{
	float f0 = 1- saturate(dot(normal, lightVec));
	return R0 + (1 - R0)(f0 * f0 * f0 * f0 * f0);
}

// BlinnPhong 光照
float3 BlinnPhong(float3 lightStrength,float3 lightVec,float3 normal,float3 toEye,Material mat)
{
	const float m = mat.Shininess * 256.f; // 粗糙度
	float3 halfVec = normal(toEye + lightVec); // 光线和入眼的中间

	float roughnessFactor = (m+8.f)*pow(max(dot(halfVec,normal),0.f),m) / 8.f; //高光系数

	float3 fresnelFactor = SchlickFresnel(mat.FresnelR0,halfVec,lightVec);

	float3 specAlbedo = fresnelFactor * roughnessFactor;

	specAlbedo = specAlbedo / (specAlbedo + 1.f);

	return (mat.DiffuseAlbedo.rgb + specAlbedo);
}