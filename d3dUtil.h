#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "d3dx12.h"
#include "MathHelper.h"
#include <dxgi1_6.h>
#include <dxgi1_4.h>
#include <assert.h>
#include "GeometryGenerator.h"
#include <type_traits>
#include "DDSTextureLoader.h"

const int gNumFrameResource = 3;
#define MaxLights 16

template<typename T>
requires(!std::is_lvalue_reference_v<T>)
T* get_rvalue_ptr(T&& v) {
	return &v;
}

class d3dUtil
{
public:
	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target
	);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& UploadBuffer
	);
};

// 光照
struct Light
{
	DirectX::XMFLOAT3 Strength = { 0.5f,0.5f,0.5f };// 光源的颜色
	float FalloffStart = 1.f; // 仅供点光源/聚光灯光源使用
	DirectX::XMFLOAT3 Direction = { 0.f,-1.f,0.f };//仅供方向光源/聚光灯光源使用
	float FalloffEnd = 10.f; // 仅供点光源/聚光灯光源使用
	DirectX::XMFLOAT3 Position = { 0.f,0.f,0.f };// // 仅供点光源/聚光灯光源使用
	float SpotPower = 64.f; //仅供聚光灯光源使用
};

struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;
	DirectX::BoundingBox Bounds;
};

// 材质
struct Material
{
	// 便于查找材质的唯一对应名称
	std::string Name;

	// 本材质的常量缓冲区索引
	int MatCBIndex = -1;

	// 漫反射纹理在srv堆中的索引
	int DiffuseSrvHeapIndex = -1;

	// 更新标记。
	int NumFramesDirty = gNumFrameResource;

	//用于着色的材质常量缓冲区数据
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.f,1.f,1.f,1.f }; // 漫反射照率
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f,0.01f,0.01f }; // 材质属性
	float Roughness = 0.25f; //粗糙度

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// 材质缓冲区
struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.f,1.f,1.f,1.f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f,0.01f,0.01f };
	float Roughness = 0.25f;
};

struct MeshGeometry
{
	std::string Name;

	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// 数据相关信息
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;// 缓冲区大小
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	//
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.SizeInBytes = VertexBufferByteSize;
		vbv.StrideInBytes = VertexByteStride;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;
		return ibv;
	}

	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};


// 纹理
struct Texture
{
	std::string Name;
	std::wstring FileName;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};