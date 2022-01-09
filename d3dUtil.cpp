#include "d3dUtil.h"
using namespace Microsoft::WRL;
Microsoft::WRL::ComPtr<ID3DBlob> d3dUtil::CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	HRESULT hr = S_OK;
	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;

	hr = D3DCompileFromFile(
		filename.c_str(),
		defines,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(),
		target.c_str(),
		compileFlags,
		0,
		&byteCode,
		&errors
	);
	if (errors != nullptr)
	{
		return nullptr;
	}
	if (hr == S_FALSE)
	{
		return nullptr;
	}
	return byteCode;
}

Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& UploadBuffer)
{
	ComPtr<ID3D12Resource> defaultBuffer;
	//
	CD3DX12_HEAP_PROPERTIES DefaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC defaultResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	device->CreateCommittedResource(
		&DefaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&defaultResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())
	);
	//
	CD3DX12_HEAP_PROPERTIES UploadHeap(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC UploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	device->CreateCommittedResource(
		&UploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(UploadBuffer.GetAddressOf())
	);
	//
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = byteSize;
	//
	CD3DX12_RESOURCE_BARRIER WriteBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST
	);
	cmdList->ResourceBarrier(1, &WriteBarrier);
	//
	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), UploadBuffer.Get(), 0, 0, 1, &subResourceData);
	//
	CD3DX12_RESOURCE_BARRIER ReadBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);
	cmdList->ResourceBarrier(1, &ReadBarrier);
	
	return defaultBuffer;
}
