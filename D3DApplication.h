#pragma once
#include <wrl.h>
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "GameTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Reflected,
	Mirrors,
	Shadow,
	AlphaTestedTreeSprites,
	Tessellation_Opaque,
	Count,
};

struct RenderItem 
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4(); // 物体局部空间相对于世界空间的世界矩阵，位置，朝向，大小
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4(); // uv

	int NumFrameDirty = gNumFrameResource; // 更新标记

	UINT ObjCBIndex = -1; // 指向常量缓冲区对应于当前渲染项中的物体常量缓冲区

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // 图元拓扑

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	MeshGeometry* Geo = nullptr; // 此渲染项参与绘制的几何体
	

	Material* Mat = nullptr;
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
	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt);
	virtual void Draw_Stencil(const GameTimer& gt);

	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateReflectedPassCB(const GameTimer& gt);

	void OnKeyboardInput(const GameTimer& gt);

	int Run();


	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
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
	void BuildDescriptorHeaps_Stencil();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildRootSignature_Stencil();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildGridGeometry();
	void BuildRoomGeometry();
	void BuildSkullGeometry();
	void BuildTreeSpritesGeometry();

	void BuildPSO();
	void BuildPSOs_Stencil();
	// CBV
	void BuildConstantBufferViews();
	// 创建FrameResource
	void BuildFrameResource();
	//
	void BuildRenderItems();
	void BuildRenderItems_Stencil();
	// 绘制每个item
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritem);
	void DrawRenderItems_Stencil(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritem);

	float     AspectRatio()const;

	// 光照begin
	
	void BuildMaterials();
	void BuildMaterials_Stencil(); // 模板

	void UpdateMaterialCBs(const GameTimer& gt);
	// 光照 end

	// 纹理begin
	
	void LoadTextures();
	void LoadTextures_Stencil();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	// 纹理end

	// 曲面细分阶段 begin

	void BuildQuadPathGeometry_Tessellation();
	void BuildRenderItems_Tessellation();
	void BuildShadersAndInputLayout_Tessellation();
	void BuildPSOs_Tessellation();

	// 曲面细分阶段 end

	int GetClientWidth()
	{
		return mClientWidth;
	}

	int GetClientHeight()
	{ 
		return mClientHeight; 
	}
public:
	static D3DApplication* Get();
private:
	void LogAdapters();   
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;
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

	int mClientWidth = 1920;
	int mClientHeight = 1080;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// GPU 与cpu 交互 废弃
	ComPtr<struct ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;
	ComPtr<IDXGISwapChain> mSpwapChain;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;

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
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	// 索引缓冲区和顶点缓冲区
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexUploadBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexUploadBuffer = nullptr;

	// pso
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	std::unordered_map<std::string, ComPtr<ID3DBlob>>			 mShaders;

	UINT vbByteSize = 0;
	UINT ibByteSize = 0;
	UINT IndexCount = 0;

	//
	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.4 * XM_PI;
	float mRadius = 5.f;

	// 
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();


	//FrameResource
	std::vector<std::unique_ptr<FrameResource>> mFrameResource;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	// render items
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mOpaqueRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	PassConstants mMainPassCB;
	PassConstants mReflectedPassCB;
	UINT mPassCbvOffset = 0;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };

	// 光照 begin

	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	// 光照 end


	// 纹理 begin
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	UINT mTextureCbvOffset = 0;
	// 纹理 end

	// game time
	GameTimer mTimer;
	bool mAppPaused = false;
	POINT mLastMousePos;

	RenderItem* mSkullRitem = nullptr;
	RenderItem* mReflectedSkullRitem = nullptr;
	RenderItem* mShadowdSkullRitem = nullptr;

	XMFLOAT3 mSkullTranslation = { 0.f,1.f,-5.f };

	// 曲面细分阶段 begin
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout_tessellation;
	// 曲面细分阶段 end

};