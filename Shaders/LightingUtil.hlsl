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
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
	float f0 = 1- saturate(dot(normal, lightVec));
	return R0 + (1 - R0)*(f0 * f0 * f0 * f0 * f0);
}

// BlinnPhong 光照
float3 BlinnPhong(float3 lightStrength,float3 lightVec,float3 normal,float3 toEye,Material mat)
{
	const float m = mat.Shininess * 256.f; // 粗糙度
	float3 halfVec = normalize(toEye + lightVec); // 光线和入眼的中间

	float roughnessFactor = (m+8.f)*pow(max(dot(halfVec,normal),0.f),m) / 8.f; //高光系数

	float3 fresnelFactor = SchlickFresnel(mat.FresnelR0,halfVec,lightVec);

	float3 specAlbedo = fresnelFactor * roughnessFactor;

	specAlbedo = specAlbedo / (specAlbedo + 1.f);

	return (mat.DiffuseAlbedo.rgb + specAlbedo);
}

// 方向光
float3 ComputeDirectionalLight(Light L,Material mat, float3 normal,float3 toEye)
{
	// 光向量与光线传播的方向正好相反
	float3 lightVec = -L.Direction;
	//通过朗伯余弦定律比例降低光强
	float ndotl = max(dot(lightVec,normal),0.f);
	float3 lightStrength = L.Strength * ndotl;

	return BlinnPhong(lightStrength,lightVec,normal,toEye,mat);
}

// 点光源
float3 ComputePointLight(Light L , Material mat,float3 pos,float3 normal,float3 toEye)
{
	//自表面指向光源的向量
	float3 lightVec = L.Position - pos;

	// 由表面到光源的距离
	float d = length(lightVec);

	// 范围检测
	if(d > L.FalloffEnd)
	return 0.f;

	// 对光向量进行规范化处理
	lightVec /=d ;

	//通过朗伯余弦定律比例降低光强
	float ndotl = max(dot(lightVec,normal),0.f);
	float3 lightStrength = L.Strength * ndotl;

	//根据距离计算光的衰减
	float att = CalcAttenuation(d,L.FalloffStart,L.FalloffEnd);
	lightStrength*=att;

	return BlinnPhong(lightStrength,lightVec,normal,toEye,mat);
}

// 聚光灯
float3 ComputeSpotLight(Light L ,Material mat ,float3 pos,float3 normal,float3 toEye)
{
	// 从表面指向光源的向量
	float3 lightVec = L.Position - pos;

	// 由表面到光源的距离
	float d = length(lightVec);

	// 范围检测
	if(d>L.FalloffEnd)
		return 0.f;
	
	// 对光向量进行规范化处理
	lightVec/=d;

	// 通过朗伯余弦定律按比例缩小光的强度
	//通过朗伯余弦定律比例降低光强
	float ndotl = max(dot(lightVec,normal),0.f);
	float3 lightStrength = L.Strength * ndotl;

	//根据距离计算光的衰减
	float att = CalcAttenuation(d,L.FalloffStart,L.FalloffEnd);
	lightStrength *= att;

	//根据聚光灯照明模型对光照进行缩放

	float spotFactor = pow(max(dot(-lightVec,L.Direction),0.f),L.SpotPower);
	lightStrength*=spotFactor;

	return BlinnPhong(lightStrength,lightVec,normal,toEye,mat);
}

float4 ComputeLighting(Light gLights[MaxLights],Material mat,
float3 pos,float3 normal,float3 toEye,
float3 shadowFactor)
{
	float3 result = 0.f;
	int i = 0;

	#if(NUM_DIR_LIGHTS>0)

for(i = 0; i < NUM_DIR_LIGHTS; ++i)
	{
		result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
	}

	#endif

	#if(NUM_POINT_LIGHTS>0)

    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
	{
		result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
		mat,normal,toEye);
	}

	#endif

	#if(NUM_SPOT_LIGHTS>0)

    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
	{
		result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
	}

	#endif

	return float4(result,0.f);
}