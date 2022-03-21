#include "D3DApplication.h"
#include <string>
#include <vector>
#include <DirectXColors.h>
#include "FrameResource.h"
#include "windowsx.h"

using namespace DirectX;
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
	switch (msg)
	{
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		return 0;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void D3DApplication::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);
	// 循环往复的获取帧资源数组中的元素;
	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % gNumFrameResource;
	mCurrentFrameResource = mFrameResource[mCurrentFrameResourceIndex].get();

	// GPU端是否已执行完处理当前帧资源的所有命令
	// 如果没有就令CPU等待,直到GPU完成命令的执行并抵达这个围栏点;
	if (mCurrentFrameResource->Fence != 0 ;mFence->GetCompletedValue()<mCurrentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, "false", false, EVENT_ALL_ACCESS);
		mFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateReflectedPassCB(gt);
}

void D3DApplication::Draw(const GameTimer& gt)
{
	auto mCurrentCommandAlloc = mCurrentFrameResource->CmdListAlloc;
	mCurrentCommandAlloc->Reset();
	mD3DCommandList->Reset(mCurrentCommandAlloc.Get(), mPSOs["opaque"].Get());


	mD3DCommandList->RSSetViewports(1, &mScreenViewport);
	mD3DCommandList->RSSetScissorRects(1, &mScissorRect);


	auto TmpCurrentBackBuffer = CurrentBackBuffer();
	auto TmpCurrentBackBufferView = CurrentBackBufferView();
	auto TmpDepthStencilView = DepthStencilView();
	mD3DCommandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(TmpCurrentBackBuffer,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));

	mD3DCommandList->ClearRenderTargetView(TmpCurrentBackBufferView, (float*)&mMainPassCB.FogColor, 0, nullptr);
	mD3DCommandList->ClearDepthStencilView(TmpDepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mD3DCommandList->OMSetRenderTargets(1, &TmpCurrentBackBufferView, true, &TmpDepthStencilView);

	// 绑定描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mD3DCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	mD3DCommandList->SetGraphicsRootSignature(mRootSignature.Get());


	//绑定常量缓冲堆
	int passCbvIndex = mPassCbvOffset + mCurrentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	// 绑定的槽号，cbv句柄
	mD3DCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);
	//auto passCB = mCurrentFrameResource->PassCB->Resource();
	//mD3DCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	//绘制命令
	mD3DCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	DrawRenderItems(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mD3DCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mD3DCommandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(TmpCurrentBackBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)));
	mD3DCommandList->Close();

	ID3D12CommandList* cmdLists[] = { mD3DCommandList.Get() };
	mD3DCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	mSpwapChain->Present(0, 0);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 添加围栏值,将命令标记到此围栏点;
	mCurrentFrameResource->Fence = ++mCurrentFence;
	// 向命令队列添加一条指令来设置一个新的围栏点
	// 由于当前的GPU正在执行绘制命令,所以在GPU处理完Signal()函数之前的所有命令以前，并不会设置此新的围栏点。
	mD3DCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void D3DApplication::Draw_Stencil(const GameTimer& gt)
{
	auto mCurrentCommandAlloc = mCurrentFrameResource->CmdListAlloc;
	mCurrentCommandAlloc->Reset();
	mD3DCommandList->Reset(mCurrentCommandAlloc.Get(), mPSOs["opaque"].Get());


	mD3DCommandList->RSSetViewports(1, &mScreenViewport);
	mD3DCommandList->RSSetScissorRects(1, &mScissorRect);


	auto TmpCurrentBackBuffer = CurrentBackBuffer();
	auto TmpCurrentBackBufferView = CurrentBackBufferView();
	auto TmpDepthStencilView = DepthStencilView();
	mD3DCommandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(TmpCurrentBackBuffer,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));

	mD3DCommandList->ClearRenderTargetView(TmpCurrentBackBufferView, (float*)&mMainPassCB.FogColor, 0, nullptr);
	mD3DCommandList->ClearDepthStencilView(TmpDepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mD3DCommandList->OMSetRenderTargets(1, &TmpCurrentBackBufferView, true, &TmpDepthStencilView);

	// 绑定描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mD3DCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	mD3DCommandList->SetGraphicsRootSignature(mRootSignature.Get());


	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	auto passCB = mCurrentFrameResource->PassCB->Resource();
	mD3DCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
	DrawRenderItems_Stencil(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mD3DCommandList->OMSetStencilRef(1);
	mD3DCommandList->SetPipelineState(mPSOs["markStencilMirrors"].Get());
	DrawRenderItems_Stencil(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Mirrors]);

	D3D12_GPU_VIRTUAL_ADDRESS ReflectionPassCbAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mD3DCommandList->SetGraphicsRootConstantBufferView(2, ReflectionPassCbAddress);
	mD3DCommandList->SetPipelineState(mPSOs["drawStencilReflections"].Get());
	DrawRenderItems_Stencil(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Reflected]);

	mD3DCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems_Stencil(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mD3DCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	mD3DCommandList->OMSetStencilRef(0);


	mD3DCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems_Stencil(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	// 绘制阴影
	mD3DCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderItems_Stencil(mD3DCommandList.Get(), mRitemLayer[(int)RenderLayer::Shadow]);



	mD3DCommandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(TmpCurrentBackBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)));
	mD3DCommandList->Close();

	ID3D12CommandList* cmdLists[] = { mD3DCommandList.Get() };
	mD3DCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	mSpwapChain->Present(0, 0);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 添加围栏值,将命令标记到此围栏点;
	mCurrentFrameResource->Fence = ++mCurrentFence;
	// 向命令队列添加一条指令来设置一个新的围栏点
	// 由于当前的GPU正在执行绘制命令,所以在GPU处理完Signal()函数之前的所有命令以前，并不会设置此新的围栏点。
	mD3DCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void D3DApplication::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritem)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto matCB = mCurrentFrameResource->MaterialCB->Resource();
	auto objectCB = mCurrentFrameResource->ObjectCB->Resource();
	for (int i = 0; i < ritem.size(); i++)
	{
		auto ri = ritem[i];

		cmdList->IASetVertexBuffers(0, 1, get_rvalue_ptr(ri->Geo->VertexBufferView()));
		cmdList->IASetIndexBuffer(get_rvalue_ptr(ri->Geo->IndexBufferView()));
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);


		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;
		{

			UINT cbvIndex = mCurrentFrameResourceIndex * (UINT)mAllRitems.size() + ri->ObjCBIndex;
			CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
			cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

			//cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		}

		{

			// 通过根描述符添加cbv
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);
			//cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);
		}
		{
			//纹理
			int TexIndex = ri->Mat->DiffuseSrvHeapIndex + mTextureCbvOffset;
			CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
			tex.Offset(TexIndex, mCbvSrvUavDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(3, tex);
		}
		//{
		//	//动态采样器
		//	cmdList->SetGraphicsRootDescriptorTable(1,

		//		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		//}
		// 添加绘制命令;
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void D3DApplication::DrawRenderItems_Stencil(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritem)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto matCB = mCurrentFrameResource->MaterialCB->Resource();
	auto objectCB = mCurrentFrameResource->ObjectCB->Resource();
	for (int i = 0; i < ritem.size(); i++)
	{
		auto ri = ritem[i];

		cmdList->IASetVertexBuffers(0, 1, get_rvalue_ptr(ri->Geo->VertexBufferView()));
		cmdList->IASetIndexBuffer(get_rvalue_ptr(ri->Geo->IndexBufferView()));
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);


		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;
		
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);
		{
			//纹理
			int TexIndex = ri->Mat->DiffuseSrvHeapIndex + mTextureCbvOffset;
			CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
			tex.Offset(TexIndex, mCbvSrvUavDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(0, tex);
		}
		// 添加绘制命令;
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void D3DApplication::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrentFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFrameDirty > 0 )
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);
			e->NumFrameDirty--;
		}
	}
}

void D3DApplication::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(get_rvalue_ptr(XMMatrixDeterminant(view)), view);
	XMMATRIX invProj = XMMatrixInverse(get_rvalue_ptr(XMMatrixDeterminant(proj)), proj);
	XMMATRIX invViewProj = XMMatrixInverse(get_rvalue_ptr(XMMatrixDeterminant(viewProj)), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();


	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrentFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void D3DApplication::UpdateCamera(const GameTimer& gt)
{
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sin(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	XMVECTOR POS = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	XMMATRIX view = XMMatrixLookAtLH(POS, target, up);

	XMStoreFloat4x4(&mView, view); 
}

void D3DApplication::UpdateReflectedPassCB(const GameTimer& gt)
{
	mReflectedPassCB = mMainPassCB;

	XMVECTOR mirrorPlane = XMVectorSet(0.f, 0.f, 1.f, 0.f);
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	for (int i = 0; i < 3; i++)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mMainPassCB.Lights[i].Direction);
		XMVECTOR reflectedLighDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mReflectedPassCB.Lights[i].Direction, reflectedLighDir);
	}

	auto currPassCB = mCurrentFrameResource->PassCB.get();
	currPassCB->CopyData(1, mReflectedPassCB);
}

void D3DApplication::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('A') & 0x8000)
	{
		mSkullTranslation.x -= 1.f * dt;
	}
	else if(GetAsyncKeyState('D') & 0x8000)
	{
		mSkullTranslation.x += 1.f * dt;
	}
	else if (GetAsyncKeyState('W') & 0x8000)
	{
		mSkullTranslation.y -= 1.f * dt;
	}
	else if (GetAsyncKeyState('S') & 0x8000)
	{
		mSkullTranslation.y += 1.f * dt;
	}

	mSkullTranslation.y = MathHelper::Max(mSkullTranslation.y, 0.f);

	XMMATRIX skullRotate = XMMatrixRotationY(0.5f * MathHelper::Pi);
	XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
	XMMATRIX skullOffset = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);

	XMMATRIX skullWorld = skullRotate * skullScale * skullOffset;
	XMStoreFloat4x4(&mSkullRitem->World, skullWorld);

	XMVECTOR mirrorPlane = XMVectorSet(0.f, 0.f, 1.f, 0.f);
	XMMATRIX R = XMMatrixReflect(mirrorPlane);
	XMStoreFloat4x4(&mReflectedSkullRitem->World, skullWorld * R);

	// shadow 阴影
	XMVECTOR shadowPlane = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMVECTOR toMainLight = -XMLoadFloat3(&mMainPassCB.Lights[0].Direction);
	XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffsetY = XMMatrixTranslation(0.f, 0.001f, 0.f);
	XMStoreFloat4x4(&mShadowdSkullRitem->World, skullWorld * S * shadowOffsetY);

	mSkullRitem->NumFrameDirty = gNumFrameResource;
	mReflectedSkullRitem->NumFrameDirty = gNumFrameResource;
	mShadowdSkullRitem->NumFrameDirty = gNumFrameResource;
}

int D3DApplication::Run()
{
	MSG msg = { 0 };
	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg,0,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mTimer.Tick();
			if (!mAppPaused)
			{
				Update(mTimer);
#if 0
				Draw(mTimer);
#else
				Draw_Stencil(mTimer);
#endif
			}
			else
			{
				Sleep(100);
			}
		}
	}
	

	return (int)msg.wParam;
}

void D3DApplication::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos = { x,y };
	SetCapture(mHWND);
}

void D3DApplication::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void D3DApplication::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta += dx;
		mPhi += dy;

		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

		mRadius += dx - dy;

		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void D3DApplication::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mAllRitems.size();
	UINT texCount = 2;
	UINT numDescriptors = (objCount + 1) * gNumFrameResource + texCount;

	mPassCbvOffset = objCount * gNumFrameResource;
	mTextureCbvOffset = (objCount + 1) * gNumFrameResource;
#if 0
	{
		// test
		numDescriptors = 1;
		mTextureCbvOffset = 0;
	}
#endif

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	mD3DDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap));

	//D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	//srvHeapDesc.NumDescriptors = 1;
	//srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	//srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	//mD3DDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap));


	auto woodCrateTex = mTextures["woodCrateTex"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;

	// 如果多个可以hDescriptor.Offset()
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
	

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 在着色器中对纹理进行采样时，它将返回特定纹理坐标处的纹理数据向量。
	
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 资源的维数
	srvDesc.Texture2D.MostDetailedMip = 0; //指定此视图中图像细节最详尽的mipmap层级的索引
	srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels; // 此视图的mipmap层级数量，以MostDetailedMip作为起始值。
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;//指定可以访问的最小mipmap层级。

	hDescriptor.Offset(mTextureCbvOffset, mCbvSrvUavDescriptorSize); 
	srvDesc.Format = woodCrateTex->GetDesc().Format; //视图格式
	mD3DDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	srvDesc.Format = grassTex->GetDesc().Format;
	mD3DDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	//动态采样器描述符
	//D3D12_SAMPLER_DESC sampleDesc = {};
	//sampleDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	//sampleDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//sampleDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//sampleDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//sampleDesc.MinLOD = 0;
	//sampleDesc.MaxLOD = D3D12_FLOAT32_MAX;
	//sampleDesc.MipLODBias = 0.f;
	//sampleDesc.MaxAnisotropy = 1;
	//sampleDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	//mD3DDevice->CreateSampler(&sampleDesc, mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3DApplication::BuildDescriptorHeaps_Stencil()
{
	UINT texCount = 5;
	mTextureCbvOffset = 0;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.NumDescriptors = texCount;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	mD3DDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap));


	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto checkboardTex = mTextures["checkboardTex"]->Resource;
	auto iceTex = mTextures["iceTex"]->Resource;
	auto white1x1Tex = mTextures["white1x1Tex"]->Resource;
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;

	// 如果多个可以hDescriptor.Offset()
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvHeap->GetCPUDescriptorHandleForHeapStart());


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 在着色器中对纹理进行采样时，它将返回特定纹理坐标处的纹理数据向量。
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 资源的维数
	srvDesc.Texture2D.MostDetailedMip = 0; //指定此视图中图像细节最详尽的mipmap层级的索引
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels; // 此视图的mipmap层级数量，以MostDetailedMip作为起始值。
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;//指定可以访问的最小mipmap层级。

	srvDesc.Format = bricksTex->GetDesc().Format; //视图格式
	mD3DDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	srvDesc.Format = checkboardTex->GetDesc().Format;
	mD3DDevice->CreateShaderResourceView(checkboardTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	srvDesc.Format = iceTex->GetDesc().Format;
	mD3DDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	srvDesc.Format = white1x1Tex->GetDesc().Format;
	mD3DDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	D3D12_RESOURCE_DESC desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = desc.Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
	mD3DDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);
}

void D3DApplication::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(mD3DDevice.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();

	int boxCBufIndex = 0;

	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;

	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	mD3DDevice->CreateConstantBufferView(
		&cbvDesc,
		mCbvHeap->GetCPUDescriptorHandleForHeapStart()
	);
}

void D3DApplication::BuildRootSignature()
{
	// 根参数
	CD3DX12_ROOT_PARAMETER rootParameter[4];

	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 
		1, 
		0//绑定的槽号
	);
	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		1 //绑定的槽号
	);
	CD3DX12_DESCRIPTOR_RANGE TexcbvTable;
	TexcbvTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		0
	);

	rootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	rootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
	//rootParameter[0].InitAsConstantBufferView(0);
	//rootParameter[1].InitAsConstantBufferView(1);
	rootParameter[2].InitAsConstantBufferView(2);
	rootParameter[3].InitAsDescriptorTable(1, &TexcbvTable,D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigdesc(
		_countof(rootParameter), 
		rootParameter, 
		(UINT)staticSamplers.size(), 
		staticSamplers.data(), 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;


	HRESULT hr = D3D12SerializeRootSignature(&rootSigdesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	assert(errorBlob== nullptr);

	mD3DDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	);
}

void D3DApplication::BuildRootSignature_Stencil()
{
	// 根参数
	CD3DX12_ROOT_PARAMETER rootParameter[4];
	CD3DX12_DESCRIPTOR_RANGE TexcbvTable;
	TexcbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);


	rootParameter[0].InitAsDescriptorTable(1, &TexcbvTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameter[1].InitAsConstantBufferView(0);
	rootParameter[2].InitAsConstantBufferView(1);
	rootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigdesc(
		_countof(rootParameter),
		rootParameter,
		(UINT)staticSamplers.size(),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;


	HRESULT hr = D3D12SerializeRootSignature(&rootSigdesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	assert(errorBlob == nullptr);

	mD3DDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	);
}

void D3DApplication::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	const D3D_SHADER_MACRO defines[] =
	{
		"FOG","1",
		NULL,NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG","1",
		"ALPHA_TEST","1",
		NULL,NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\GSMain_12.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\GSMain_12.hlsl", defines, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\GSMain_12.hlsl", alphaTestDefines, "PS", "ps_5_1");
	
	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\GSTreeSpritesMain_12.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\GSTreeSpritesMain_12.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\GSTreeSpritesMain_12.hlsl", alphaTestDefines, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void D3DApplication::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	//输入布局描述
	psoDesc.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	// 光栅器
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// 混合
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	// 指定图元的拓扑类型
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// 同时所用的渲染目标数量
	psoDesc.NumRenderTargets = 1;
	// 渲染目标的格式
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	// 设置多重采样样本(个数最多32个)
	psoDesc.SampleMask = UINT_MAX;
	// 多重采样对每个像素采样的数量
	psoDesc.SampleDesc.Count = b4xMassState ? 4 : 1;
	// 多重采样对每个像素采样的质量
	psoDesc.SampleDesc.Quality = b4xMassState ? (m4xMsaaQulity - 1) : 0;
	// 指定用于配置深度/模板测试的模板/深度状态
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// 深度/模板缓冲区的格式
	psoDesc.DSVFormat = mDepthStencilFormat;
	//创建pso 对象
	mD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["opaque"]));

	// 透明pso

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = psoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	
	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	mD3DDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"]));


	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = psoDesc;
	alphaTestedPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	mD3DDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"]));
}

void D3DApplication::BuildPSOs_Stencil()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	// pso for opaque objects
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = b4xMassState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = b4xMassState ? (m4xMsaaQulity - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	mD3DDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"]));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	mD3DDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"]));

	CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
	mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC mirrorDSS;
	mirrorDSS.DepthEnable = true;
	mirrorDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	mirrorDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	mirrorDSS.StencilEnable = true;
	mirrorDSS.StencilReadMask = 0xff;
	mirrorDSS.StencilWriteMask = 0xff;

	mirrorDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	// We are not rendering backfacing polygons, so these settings do not matter.
	mirrorDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC markMirrorsPsoDesc = opaquePsoDesc;
	markMirrorsPsoDesc.BlendState = mirrorBlendState;
	markMirrorsPsoDesc.DepthStencilState = mirrorDSS;
	mD3DDevice->CreateGraphicsPipelineState(&markMirrorsPsoDesc, IID_PPV_ARGS(&mPSOs["markStencilMirrors"]));


	D3D12_DEPTH_STENCIL_DESC reflectionDSS;
	reflectionDSS.DepthEnable = true;
	reflectionDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectionDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	reflectionDSS.StencilEnable = true;
	reflectionDSS.StencilWriteMask = 0xff;
	reflectionDSS.StencilReadMask = 0xff;

	reflectionDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	reflectionDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawReflectionPsoDesc = opaquePsoDesc;
	drawReflectionPsoDesc.DepthStencilState = reflectionDSS;
	drawReflectionPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	drawReflectionPsoDesc.RasterizerState.FrontCounterClockwise = true;
	mD3DDevice->CreateGraphicsPipelineState(&drawReflectionPsoDesc, IID_PPV_ARGS(&mPSOs["drawStencilReflections"]));


	// pso for shadow objects
	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	mD3DDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"]));

	// pso for tree sprites
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};

	treeSpritePsoDesc.GS = 
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};

	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};

	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	mD3DDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"]));
}

void D3DApplication::BuildConstantBufferViews()
{
	// 计算常量缓冲区大小;
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	// 绘制元素个数;
	UINT objCount = (UINT)mAllRitems.size();

	for (size_t i = 0; i < gNumFrameResource; i++)
	{
		auto objctCB = mFrameResource[i]->ObjectCB->Resource();
		for (UINT j = 0; j < objCount; j++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objctCB->GetGPUVirtualAddress();

			cbAddress += j * objCBByteSize;
			int heapIndex = i * objCount + j;
			
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			mD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	UINT TextIndex = 0;
	for (int frameIndex = 0; frameIndex < gNumFrameResource; ++frameIndex)
	{
		auto passCB = mFrameResource[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		int heapIndex = mPassCbvOffset + frameIndex;
		TextIndex = heapIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		mD3DDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void D3DApplication::BuildFrameResource()
{
	for (int i = 0 ;i<gNumFrameResource;i++)
	{
		mFrameResource.push_back(std::make_unique<FrameResource>(mD3DDevice.Get(), 2, (UINT)mAllRitems.size(),(UINT)mMaterials.size()));
	}
}

void D3DApplication::BuildBoxGeometry()
{

	GeometryGenerator geoGen;

	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);


	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.BaseVertexLocation = 0;


	std::vector<Vertex> vertices(box.Verteices.size());

	for (size_t i = 0; i < box.Verteices.size(); ++i)
	{
		vertices[i].Position = box.Verteices[i].Position;
		vertices[i].Normal = box.Verteices[i].Normal;
		vertices[i].TexC = box.Verteices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "ShapGeo";

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void D3DApplication::BuildGridGeometry()
{
	GeometryGenerator geoGen;

	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);


	std::vector<Vertex> vertices(grid.Verteices.size());
	for (size_t i = 0; i < grid.Verteices.size(); ++i)
	{
		auto& p = grid.Verteices[i].Position;
		vertices[i].Position = p;
		vertices[i].Position.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Verteices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void D3DApplication::BuildRoomGeometry()
{
	std::array<Vertex, 20> vertices =
	{
		Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f),
		Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),
		Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int16_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;
	
	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;


	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void D3DApplication::BuildSkullGeometry()
{
	std::ifstream fin("Models/car.txt");

	if (!fin)
	{
		MessageBoxW(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void D3DApplication::BuildTreeSpritesGeometry()
{
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 16;

	std::array<TreeSpriteVertex, 16> verteices;

	for (UINT i = 0; i < treeCount; i++)
	{
		float x = MathHelper::RandF(-45.f, 45.f);
		float z = MathHelper::RandF(-45.f, 45.f);
		float y = 0;

		verteices[i].Pos = XMFLOAT3(x, y, z);
		verteices[i].Size = XMFLOAT2(5.f, 5.f);
	}

	std::array<std::uint16_t, 16> indics =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};
	
	const UINT vbByteSize = (UINT)verteices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indics.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), verteices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indics.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), verteices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3DDevice.Get(),
		mD3DCommandList.Get(), indics.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indics.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}

void D3DApplication::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 1;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;


	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 1;
	wirefence->DiffuseSrvHeapIndex = 0;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.f, 1.f, 1.f, 0.5f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.f;


	mMaterials["grass"] = std::move(grass);
	mMaterials["wirefence"] = std::move(wirefence);
}

void D3DApplication::BuildMaterials_Stencil()
{
	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertitle = std::make_unique<Material>();
	checkertitle->Name = "checkertitle";
	checkertitle->MatCBIndex = 1;
	checkertitle->DiffuseSrvHeapIndex = 1;
	checkertitle->DiffuseAlbedo = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	checkertitle->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkertitle->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material>();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrvHeapIndex = 3;
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.f, 0.f, 0.f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.2f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 5;
	treeSprites->DiffuseSrvHeapIndex = 4;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;


	mMaterials["bricks"] = std::move(bricks);
	mMaterials["checkertitle"] = std::move(checkertitle);
	mMaterials["icemirror"] = std::move(icemirror);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["shadowMat"] = std::move(shadowMat);
	mMaterials["treeSprites"] = std::move(treeSprites);
}

void D3DApplication::BuildRenderItems()
{
	UINT objCBIndex = 0;
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(0.0f, 5.0f, 0.0f));
	boxRitem->ObjCBIndex = objCBIndex++;
	boxRitem->Geo = mGeometries["ShapGeo"].get();
	boxRitem->Mat = mMaterials["wirefence"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem.get());

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->ObjCBIndex = objCBIndex++;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	
	mRitemLayer[(int)RenderLayer::Transparent].push_back(gridRitem.get());

	mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(gridRitem));
	for (auto& e : mAllRitems)
	{
		mOpaqueRitems.push_back(e.get());
	}
}

void D3DApplication::BuildRenderItems_Stencil()
{
	auto floorRitem = std::make_unique<RenderItem>();
	floorRitem->World = MathHelper::Identity4x4();
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["checkertitle"].get();
	floorRitem->Geo = mGeometries["roomGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

	auto wallsRitem = std::make_unique<RenderItem>();
	wallsRitem->World = MathHelper::Identity4x4();
	wallsRitem->TexTransform = MathHelper::Identity4x4();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = mMaterials["bricks"].get();
	wallsRitem->Geo = mGeometries["roomGeo"].get();
	wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = MathHelper::Identity4x4();
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	mSkullRitem = skullRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());

	auto reflectedSkullRitem = std::make_unique<RenderItem>();
	*reflectedSkullRitem = *skullRitem;
	reflectedSkullRitem->ObjCBIndex = 3;
	mReflectedSkullRitem = reflectedSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkullRitem.get());


	auto mirrorRitem = std::make_unique<RenderItem>();
	mirrorRitem->World = MathHelper::Identity4x4();
	mirrorRitem->TexTransform = MathHelper::Identity4x4();
	mirrorRitem->ObjCBIndex = 4;
	mirrorRitem->Mat = mMaterials["icemirror"].get();
	mirrorRitem->Geo = mGeometries["roomGeo"].get();
	mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
	mRitemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());

	auto shadowSkullRitem = std::make_unique<RenderItem>();
	*shadowSkullRitem = *skullRitem;
	shadowSkullRitem->ObjCBIndex = 5;
	shadowSkullRitem->Mat = mMaterials["shadowMat"].get();
	mShadowdSkullRitem = shadowSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowSkullRitem.get());

	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 6;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	treeSpritesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());


	mAllRitems.push_back(std::move(floorRitem));
	mAllRitems.push_back(std::move(wallsRitem));
	mAllRitems.push_back(std::move(skullRitem));
	mAllRitems.push_back(std::move(reflectedSkullRitem));
	mAllRitems.push_back(std::move(mirrorRitem));
	mAllRitems.push_back(std::move(shadowSkullRitem));
	mAllRitems.push_back(std::move(treeSpritesRitem));
}

float D3DApplication::AspectRatio() const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

void D3DApplication::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrentFrameResource->MaterialCB.get();

	for (auto& m : mMaterials)
	{
		Material* mat = m.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);
			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));
			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
			mat->NumFramesDirty--;
		}
	}
}

void D3DApplication::LoadTextures()
{
	auto woodCrateTex = std::make_unique<Texture>();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->FileName = L"Textures/WireFence.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), woodCrateTex->FileName.c_str(),
		woodCrateTex->Resource, woodCrateTex->UploadHeap);

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->FileName = L"Textures/grass.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), grassTex->FileName.c_str(),
		grassTex->Resource, grassTex->UploadHeap);

	mTextures[woodCrateTex->Name] = std::move(woodCrateTex);
	mTextures[grassTex->Name] = std::move(grassTex);
}

void D3DApplication::LoadTextures_Stencil()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->FileName = L"Textures/bricks3.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), bricksTex->FileName.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap);

	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "checkboardTex";
	checkboardTex->FileName = L"Textures/checkboard.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), checkboardTex->FileName.c_str(),
		checkboardTex->Resource, checkboardTex->UploadHeap);

	auto iceTex = std::make_unique<Texture>();
	iceTex->Name = "iceTex";
	iceTex->FileName = L"Textures/ice.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), iceTex->FileName.c_str(),
		iceTex->Resource, iceTex->UploadHeap);

	auto white1x1Tex = std::make_unique<Texture>();
	white1x1Tex->Name = "white1x1Tex";
	white1x1Tex->FileName = L"Textures/white1x1.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), white1x1Tex->FileName.c_str(),
		white1x1Tex->Resource, white1x1Tex->UploadHeap);

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->FileName = L"Textures/treeArray2.dds";
	DirectX::CreateDDSTextureFromFile12(mD3DDevice.Get(),
		mD3DCommandList.Get(), treeArrayTex->FileName.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap);

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[checkboardTex->Name] = std::move(checkboardTex);
	mTextures[iceTex->Name] = std::move(iceTex);
	mTextures[white1x1Tex->Name] = std::move(white1x1Tex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> D3DApplication::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

void D3DApplication::BuildQuadPathGeometry_Tessellation()
{
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

	//新建命令
	CreateCommandObjects();
	//新建交换缓冲区
	CreateSwapChain();
	//新建描述符表
	CreateRtvAndDsvDescriptorHeaps();
	//
	OnResize();

	mD3DCommandList->Reset(mD3DCommandAllocator.Get(), nullptr);

	LoadTextures_Stencil();
	BuildRootSignature_Stencil();
	BuildDescriptorHeaps_Stencil();
	BuildShadersAndInputLayout();
	BuildRoomGeometry();
	BuildSkullGeometry();
	BuildTreeSpritesGeometry();
	BuildMaterials_Stencil();
	BuildRenderItems_Stencil();
	BuildFrameResource();
	BuildPSOs_Stencil();

	mD3DCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mD3DCommandList.Get() };
	mD3DCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();
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
		OutputDebugStringW(text.c_str());

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

		::OutputDebugStringW(text.c_str());
	}
}

float D3DApplication::GetHillsHeight(float x, float z) const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 D3DApplication::GetHillsNormal(float x, float z) const
{
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
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
	// bufferDesc 这个结构体描述了带创建后台缓冲区的属性。
	sd.BufferDesc.Width = mClientWidth; // 缓冲区分辨率的宽度
	sd.BufferDesc.Height = mClientHeight; //缓冲区分辨率的高度
	sd.BufferDesc.RefreshRate.Numerator = 60; // 刷新频率
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat; // 缓冲区的显示格式
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; // 逐行扫描 vs 隔行扫描
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED; // 图像如何相对于屏幕进行拉伸
	// SampleDesc 多重采样的质量级别以及对每个像素的采样次数。
	sd.SampleDesc.Count = b4xMassState ? 4 : 1; 
	sd.SampleDesc.Quality = b4xMassState ? (m4xMsaaQulity - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 由于我们要将数据渲染至后台缓冲区，因此指定为DXGI_USAGE_RENDER_TARGET_OUTPUT
	sd.BufferCount = SwapChainBufferCount;//交换链中所用缓冲区数量。
	sd.OutputWindow = mHWND;// 渲染窗口的句柄
	sd.Windowed = true;//指定为true，程序将在窗口模式下运行，指定为false 则采用全屏。
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;//可选标记。如果指定为DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH，那么当程序切换为全屏的模式时，将采用当前桌面的显示模式

	mD3DFactory->CreateSwapChain(
		mD3DCommandQueue.Get(), // 指向ID3D12CommandQueue接口的指针。
		&sd, // 指向描述符交换链的结构体指针。
		mSpwapChain.GetAddressOf());//返回所创建的交换链接口。
}

void D3DApplication::OnResize()
{
	assert(mD3DDevice);
	assert(mSpwapChain);
	assert(mD3DCommandAllocator);


	mD3DCommandList->Reset(mD3DCommandAllocator.Get(), nullptr);

	for (int i = 0; i < SwapChainBufferCount; ++i)
	{
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();
	
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

	// 创建深度/模板缓冲区描述符
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = b4xMassState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = b4xMassState ? (m4xMsaaQulity - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	CD3DX12_HEAP_PROPERTIES DSHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	mD3DDevice->CreateCommittedResource(
		&DSHeap,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	mD3DDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	CD3DX12_RESOURCE_BARRIER ResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mD3DCommandList->ResourceBarrier(1, &ResBarrier);

	// Execute the resize commands.
	mD3DCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mD3DCommandList.Get() };
	mD3DCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 1.f, 1000.f);
	XMStoreFloat4x4(&mProj, P);

}

void D3DApplication::FlushCommandQueue()
{
	mCurrentFence++;
	mD3DCommandQueue->Signal(mFence.Get(), mCurrentFence); // GPU设置当前Fence值
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventExW(nullptr, L"false", false, EVENT_ALL_ACCESS);
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

ID3D12Resource* D3DApplication::CurrentBackBuffer() const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}
