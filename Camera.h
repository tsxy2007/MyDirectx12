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


	// ��ȡ����Ļ�����
	DirectX::XMVECTOR GetRight()const;
	DirectX::XMFLOAT3 GetRight3f() const;
	
	DirectX::XMVECTOR GetUp()const;
	DirectX::XMFLOAT3 GetUp3f() const;

	DirectX::XMVECTOR GetLook()const;
	DirectX::XMFLOAT3 GetLook3f() const;


	//��ȡ��׶������
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// ��ȡ�ù۲�ռ������ʾ�Ľ���Զƽ��Ĵ�С
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;

	// ������׶��
	void SetLens(float fovY, float aspect, float zn, float zf);

	//ͨ��lookat�����Ĳ���������������ռ�
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(DirectX::XMFLOAT3& pos, DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& worldUp);

	// ��ȡ�۲������ͶӰ����
	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj()const;

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// �������������d��������ƽ��(strafe)��ǰ���ƶ�(walk)
	void Strafe(float d);
	void Walk(float d);

	// �������������ת
	void Pitch(float angle);
	void RotateY(float angle);
	
	//�޸��������λ���볯��֮�󣬵��ô˺��������¹����۲����
	void UpdateViewMatrix();
private:

	DirectX::XMFLOAT3 mPosition = { 0.0f,0.0f,0.f };
	DirectX::XMFLOAT3 mRight = { 1.0f,0.0f,0.f };
	DirectX::XMFLOAT3 mUp = { 0.f,1.f,0.f };
	DirectX::XMFLOAT3 mLook = { 0.f,0.f,1.f };

	//��׶�������
	float mNearZ = 0.f;
	float mFarZ = 0.f;
	float mAspect = 0.f;
	float mFovY = 0.f;
	float mNearWindowHeight = 0.f;
	float mFarWindowHeight = 0.f;

	bool mViewDirty = true;

	// ����۲������ͶӰ����
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

