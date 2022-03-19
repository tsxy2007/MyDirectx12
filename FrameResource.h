#pragma once
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct Vertex
{
	Vertex() = default;
	Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v):
		Position(x,y,z),
		Normal(nx,ny,nz),
		TexC(u,v)
	{

	}
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};


struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.f,0.f,0.f,1.f };

	// fog
	DirectX::XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float gFogStart = 2.0f;
	float gFogRange = 150.0f;
	DirectX::XMFLOAT2 cbPerObjectPad2;
	Light Lights[MaxLights];
};

class FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount,UINT materialCount);
	FrameResource(ID3D12Device* device, UINT passCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();
	
	//
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	//
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	//
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	//
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
	//
	UINT64 Fence = 0;
};

