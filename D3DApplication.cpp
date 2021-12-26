#include "D3DApplication.h"
#include <string>
#include <vector>

void Wchar_tToString(std::string& szDst, wchar_t* wchar)
{
	wchar_t* wText = wchar;
	DWORD dwNum = WideCharToMultiByte(CP_OEMCP, NULL, wText, -1, NULL, 0, NULL, FALSE);// WideCharToMultiByte的运用
	char* psText; // psText为char*的临时数组，作为赋值给std::string的中间变量
	psText = new char[dwNum];
	WideCharToMultiByte(CP_OEMCP, NULL, wText, -1, psText, dwNum, NULL, FALSE);// WideCharToMultiByte的再次运用
	szDst = psText;// std::string赋值
	delete[]psText;// psText的清除
}

D3DApplication::D3DApplication() 
	:mInstance(nullptr)
{

}

D3DApplication::~D3DApplication()
{
	if (mD3DDevice!=nullptr)
	{
		FlushCommandQueue();
	}
}

LRESULT D3DApplication::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

D3DApplication* D3DApplication::Get()
{
	static D3DApplication* Instance = nullptr;
	if (Instance == nullptr)
	{
		Instance = new D3DApplication;
	}
	return Instance;
}

void D3DApplication::InitInstance(HINSTANCE InInstance, HWND inHWND)
{
	mInstance = InInstance;
	mHWND = inHWND;
}

void D3DApplication::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();
	}
#endif
	// 创建D3DFactory
	CreateDXGIFactory(IID_PPV_ARGS(&mD3DFactory)); 

	//创建Device
	HRESULT hr = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&mD3DDevice)
	);
	//如果创建失败
	if (FAILED(hr))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		mD3DFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));

		D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&mD3DDevice)
			);
	}

	//创建Fence
	mD3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));

	// 计算描述符大小
	mRtvDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptroSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = mD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//设置MSAA 抗锯齿
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS mQualityLevels;
	mQualityLevels.Format = mBackBufferFormat;
	mQualityLevels.SampleCount = 4;
	mQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	mQualityLevels.NumQualityLevels = 0;

	mD3DDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&mQualityLevels,
		sizeof(mQualityLevels));
#ifdef _DEBUG
	LogAdapters();
#endif

	//新建命令相关
	CreateCommandObjects();
	//新建交换缓冲区
	CreateSwapChain();
	//新建描述符表
	CreateRtvAndDsvDescriptorHeaps();
}

void D3DApplication::LogAdapters()
{
	IDXGIAdapter* Adapter = nullptr;
	LUID Luid = mD3DDevice->GetAdapterLuid();
	HRESULT hr = mD3DFactory->EnumAdapterByLuid(Luid, IID_PPV_ARGS(&Adapter));
	if (!FAILED(hr))
	{
		DXGI_ADAPTER_DESC desc;
		Adapter->GetDesc(&desc);

		std::string Current_Path;
		Wchar_tToString(Current_Path, desc.Description);
		std::string text = "***Adapter: ";
		text += Current_Path;
		text += "\n";

		LogAdapterOutputs(Adapter);
	}
}

void D3DApplication::LogAdapterOutputs(IDXGIAdapter* Adapter)
{
	IDXGIOutput* Output = nullptr;

	UINT i = 0;
	while (Adapter->EnumOutputs(i, &Output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		Output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(Output, mBackBufferFormat);

		//ReleaseCom(output);

		++i;
	}
}

void D3DApplication::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{

	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}

void D3DApplication::CreateCommandObjects()
{
	// 创建命令队列
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	mD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mD3DCommandQueue));

	//创建命令分配器
	
	mD3DDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&mD3DCommandAllocator)
	);

	//创建命令列表
	mD3DDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mD3DCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(mD3DCommandList.GetAddressOf())
	);

	mD3DCommandList->Close();
}

void D3DApplication::CreateSwapChain()
{
	mSpwapChain.Reset();
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = b4xMassState ? 4 : 1;
	sd.SampleDesc.Quality = b4xMassState ? (m4xMsaaQulity - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mHWND;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	mD3DFactory->CreateSwapChain(
		mD3DCommandQueue.Get(), 
		&sd, 
		mSpwapChain.GetAddressOf());
}

void D3DApplication::OnResize()
{
	assert(mD3DDevice);
	assert(mSpwapChain);
	assert(mD3DCommandAllocator);

	FlushCommandQueue();

	mD3DCommandList->Reset(mD3DCommandAllocator.Get(), nullptr);

	for (int i = 0; i < SwapChainBufferCount; ++i)
	{
		mSwapChainBuffer[i].Reset();
	}
	//Goto: depthstencilBuffer
	
	mSpwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	);

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		mSpwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i]));
		mD3DDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}


}

void D3DApplication::FlushCommandQueue()
{
	mCurrentFence++;
	mD3DCommandQueue->Signal(mFence.Get(), mCurrentFence);
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, L"false", false, EVENT_ALL_ACCESS);
		mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void D3DApplication::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	mD3DDevice->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(mRtvHeap.GetAddressOf())
	);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	mD3DDevice->CreateDescriptorHeap(
		&dsvHeapDesc,
		IID_PPV_ARGS(mDsvHeap.GetAddressOf())
	);

}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApplication::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApplication::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
