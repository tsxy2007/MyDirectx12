#pragma once
#include <wrl.h>
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <dxgi1_6.h>
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <d3d12.h>
#include <assert.h>

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
protected:
	void CreateCommandObjects();
	void CreateSwapChain();
	void OnResize();
	void FlushCommandQueue();
	virtual void CreateRtvAndDsvDescriptorHeaps();
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;
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

	// GPU 与cpu 交互
	ComPtr<struct ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;
	ComPtr<IDXGISwapChain> mSpwapChain;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
};