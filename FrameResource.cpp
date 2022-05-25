#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT maxInstanceCount, UINT materialCount)
{
	device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	);
	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	//ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	//MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
	MaterialBuffer = std::make_unique<UploadBuffer<MatrialData>>(device, materialCount, false);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);
}

FrameResource::~FrameResource()
{

}
