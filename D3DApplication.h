#pragma once
#include <wrl.h>
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};


using Microsoft::WRL::ComPtr;
class D3DApplication
{
public:
	D3DApplication();
	virtual ~D3DApplication();
public:
	virtual void InitInstance(HINSTANCE InInstance, HWND inHWND);
	virtual void InitDirect3D();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void Update();
	virtual void Draw();

protected:
	void CreateCommandObjects();
	void CreateSwapChain();
	void OnResize();
	void FlushCommandQueue();
	virtual void CreateRtvAndDsvDescriptorHeaps();
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;
	ID3D12Resource* CurrentBackBuffer()const;

public:
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

	float     AspectRatio()const;
public:
	static D3DApplication* Get();
private:
	void LogAdapters();   
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
private:
	//ComPtr<ID3D12Device*> mDevice;
	HINSTANCE mInstance = nullptr;
	HWND mHWND = nullptr;
	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;

	
	ComPtr<struct IDXGIFactory4> mD3DFactory;
	ComPtr<struct ID3D12Device> mD3DDevice;

	ComPtr<ID3D12CommandQueue> mD3DCommandQueue;
	ComPtr<ID3D12CommandAllocator> mD3DCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> mD3DCommandList;

	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	ComPtr<ID3D12DescriptorHeap> mDsvHeap;


	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptroSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	UINT m4xMsaaQulity = 0;
	bool b4xMassState = false;

	int mClientWidth = 640;
	int mClientHeight = 400;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// GPU 与cpu 交互
	ComPtr<struct ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;
	ComPtr<IDXGISwapChain> mSpwapChain;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;

	//设置屏幕
	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;


private:
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	// 索引缓冲区和顶点缓冲区
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	UINT vbByteSize = 0;
	UINT ibByteSize = 0;
	UINT IndexCount = 0;

	//
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	// 
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};