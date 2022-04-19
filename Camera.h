#pragma once
#include "d3dUtil.h"
class Camera
{
public:
	Camera();
	~Camera();

	DirectX::XMVECTOR GetPosition()const;
	DirectX::XMFLOAT3 GetPosition3f()const;
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& v);


	// 获取相机的基向量
	DirectX::XMVECTOR GetRight()const;
	DirectX::XMFLOAT3 GetRight3f() const;
	
	DirectX::XMVECTOR GetUp()const;
	DirectX::XMFLOAT3 GetUp3f() const;

	DirectX::XMVECTOR GetLook()const;
	DirectX::XMFLOAT3 GetLook3f() const;


	//获取视锥体属性
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// 获取用观察空间坐标表示的近，远平面的大小
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;

	// 设置视锥体
	void SetLens(float fovY, float aspect, float zn, float zf);

	//通过lookat方法的参数来定义摄像机空间
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(DirectX::XMFLOAT3& pos, DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& worldUp);

	// 获取观察矩阵与投影矩阵
	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj()const;

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// 将摄像机按距离d进行左右平移(strafe)或前后移动(walk)
	void Strafe(float d);
	void Walk(float d);

	// 将摄像机进行旋转
	void Pitch(float angle);
	void RotateY(float angle);
	
	//修改摄像机的位置与朝向之后，调用此函数来重新构建观察矩阵
	void UpdateViewMatrix();
private:

	DirectX::XMFLOAT3 mPosition = { 0.0f,0.0f,0.f };
	DirectX::XMFLOAT3 mRight = { 1.0f,0.0f,0.f };
	DirectX::XMFLOAT3 mUp = { 0.f,1.f,0.f };
	DirectX::XMFLOAT3 mLook = { 0.f,0.f,1.f };

	//视锥体的属性
	float mNearZ = 0.f;
	float mFarZ = 0.f;
	float mAspect = 0.f;
	float mFovY = 0.f;
	float mNearWindowHeight = 0.f;
	float mFarWindowHeight = 0.f;

	bool mViewDirty = true;

	// 缓存观察矩阵与投影矩阵
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

