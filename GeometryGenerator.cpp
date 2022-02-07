#include "GeometryGenerator.h"
#include <algorithm>
using namespace DirectX;

GeometryGenerator::MeshData GeometryGenerator::CreateBox(float width, float height, float depth, uint32 numSubdivisions)
{
	MeshData meshData;

	//
	// Create the vertices.
	//

	Vertex v[24];

	float w2 = 0.5f * width;
	float h2 = 0.5f * height;
	float d2 = 0.5f * depth;

	// Fill in the front face vertex data.
	v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the back face vertex data.
	v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the top face vertex data.
	v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
	v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
	v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	meshData.Verteices.assign(&v[0], &v[24]);

	//
	// Create the indices.
	//

	uint32 i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices32.assign(&i[0], &i[36]);

	// Put a cap on the number of subdivisions.
	numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

	for (uint32 i = 0; i < numSubdivisions; ++i)
		Subdivide(meshData);

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
{
	MeshData meshData;

	Vertex topVertex(0.f, +radius, 0.f, 0.f, +1.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f);
	Vertex bottomVertex(0.f, -radius, 0.f, 0.f, -1.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

	meshData.Verteices.push_back(topVertex);
	
	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.f * XM_PI / sliceCount;

	for (uint32 i = 1; i <= stackCount-1; i++)
	{
		float phi = i * phiStep;
		for (uint32 j = 0; j <= sliceCount; j++)
		{
			float theta = j * thetaStep;

			Vertex v;

			v.Position.x = radius * sinf(phi) * cosf(theta);
			v.Position.y = radius * cosf(phi);
			v.Position.z = radius * sinf(phi) * sinf(theta);

			v.TangentU.x = 0.f;
			v.TangentU.y = 0.f;
			v.TangentU.z = 1.f;

			XMVECTOR T = XMLoadFloat3(&v.Position);
			XMVECTOR B = XMLoadFloat3(&v.TangentU);
			XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));
			XMStoreFloat3(&v.TangentU, N);

			XMVECTOR p = XMLoadFloat3(&v.Position);
			XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			v.TexC.x = theta / XM_2PI;
			v.TexC.y = phi / XM_PI;

			meshData.Verteices.push_back(v);
		}
	}

	meshData.Verteices.push_back(bottomVertex);

	for (uint32 i = 1; i <= sliceCount; i++)
	{
		meshData.Indices32.push_back(0);
		meshData.Indices32.push_back(i + 1);
		meshData.Indices32.push_back(i);
	}

	uint32 baseIndex = 1;
	uint32 ringVertexCount = sliceCount + 1;

	for (uint32 i = 0; i < stackCount - 2; i++)
	{
		for (uint32 j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
			meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);

			meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
			meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	uint32 southPoleIndex = (uint32)meshData.Verteices.size() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (uint32 i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(southPoleIndex);
		meshData.Indices32.push_back(baseIndex + i);
		meshData.Indices32.push_back(baseIndex + i + 1);
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGeoSphere(float radius, uint32 numSubdicisions)
{
	MeshData meshData;

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
{
	MeshData meshData;

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGrid(float width, float depth, uint32 m, uint32 n)
{
	MeshData meshData;

	uint32 vertexCount = m * n;
	uint32 faceCount = (m - 1) * (n - 1) * 2;

	float halfWidth = 0.5f * width;
	float halfDepth = 0.5f * depth;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.f / (n - 1);
	float dv = 1.f / (m - 1);

	meshData.Verteices.resize(vertexCount);

	for (uint32 i = 0; i < m; i++)
	{
		float z = halfDepth - i * dz;
		for (uint32 j = 0; j < n; j++)
		{
			float x = -halfWidth + j * dx;

			meshData.Verteices[i * n + j].Position = XMFLOAT3(x, 0.f, z);
			meshData.Verteices[i * n + j].Normal = XMFLOAT3(0.f, 1.f, 0.f);
			meshData.Verteices[i * n + j].TangentU = XMFLOAT3(1.f, 0.f, 0.f);

			meshData.Verteices[i * n + j].TexC.x = j * du;
			meshData.Verteices[i * n + j].TexC.y = i * dv;
		}
	}

	meshData.Indices32.resize(faceCount * 3); // 3 indices per face

	uint32 k = 0;
	for (uint32 i = 0; i < m - 1; ++i)
	{
		for (uint32 j = 0; j < n - 1; ++j)
		{
			meshData.Indices32[k] = i * n + j;
			meshData.Indices32[k + 1] = i * n + j + 1;
			meshData.Indices32[k + 2] = (i + 1) * n + j;

			meshData.Indices32[k + 3] = (i + 1) * n + j;
			meshData.Indices32[k + 4] = i * n + j + 1;
			meshData.Indices32[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateQuad(float x, float y, float w, float h, float depth)
{
	MeshData meshData;

	meshData.Verteices.resize(4);
	meshData.Indices32.resize(6);

	// ÆÁÄ»¿Õ¼ä×ø±êÏµ
	meshData.Verteices[0] = Vertex(
		x, y - h, depth,
		0.f, 0.f, -1.f,
		1.f, 0.f, 0.f,
		0.f, 1.f
	);

	meshData.Verteices[1] = Vertex(
		x, y, depth,
		0.f, 0.f, -1.f,
		1.f, 0.f, 0.f,
		0.f, 0.f
	);

	meshData.Verteices[2] = Vertex(
		x + w, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f);

	meshData.Verteices[3] = Vertex(
		x + w, y - h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f);

	meshData.Indices32[0] = 0;
	meshData.Indices32[1] = 1;
	meshData.Indices32[2] = 2;

	meshData.Indices32[3] = 0;
	meshData.Indices32[4] = 2;
	meshData.Indices32[5] = 3;

	return meshData;
}

void GeometryGenerator::Subdivide(MeshData& meshData)
{
	MeshData inputCopy = meshData;
	meshData.Verteices.resize(0);
	meshData.Indices32.resize(0);

	uint32 numTris = (uint32)inputCopy.Indices32.size() / 3;
	for (uint32 i = 0; i < numTris; ++i)
	{
		Vertex v0 = inputCopy.Verteices[inputCopy.Indices32[i * 3 + 0]];
		Vertex v1 = inputCopy.Verteices[inputCopy.Indices32[i * 3 + 1]];
		Vertex v2 = inputCopy.Verteices[inputCopy.Indices32[i * 3 + 2]];

		Vertex m0 = MidPoint(v0, v1);
		Vertex m1 = MidPoint(v1, v2);
		Vertex m2 = MidPoint(v0, v2);


		meshData.Verteices.push_back(v0); // 0
		meshData.Verteices.push_back(v1); // 1
		meshData.Verteices.push_back(v2); // 2
		meshData.Verteices.push_back(m0); // 3
		meshData.Verteices.push_back(m1); // 4
		meshData.Verteices.push_back(m2); // 5
		
		meshData.Indices32.push_back(i * 6 + 0);
		meshData.Indices32.push_back(i * 6 + 3);
		meshData.Indices32.push_back(i * 6 + 5);

		meshData.Indices32.push_back(i * 6 + 3);
		meshData.Indices32.push_back(i * 6 + 4);
		meshData.Indices32.push_back(i * 6 + 5);

		meshData.Indices32.push_back(i * 6 + 5);
		meshData.Indices32.push_back(i * 6 + 4);
		meshData.Indices32.push_back(i * 6 + 2);

		meshData.Indices32.push_back(i * 6 + 3);
		meshData.Indices32.push_back(i * 6 + 1);
		meshData.Indices32.push_back(i * 6 + 4);
	}
}

GeometryGenerator::Vertex GeometryGenerator::MidPoint(const Vertex& v0, const Vertex& v1)
{
	XMVECTOR p0 = XMLoadFloat3(&v0.Position);
	XMVECTOR p1 = XMLoadFloat3(&v1.Position);

	XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
	XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

	XMVECTOR tan0 = XMLoadFloat3(&v0.TangentU);
	XMVECTOR tan1 = XMLoadFloat3(&v1.TangentU);

	XMVECTOR tex0 = XMLoadFloat2(&v0.TexC);
	XMVECTOR tex1 = XMLoadFloat2(&v1.TexC);

	XMVECTOR pos = 0.5f * (p0 + p1);
	XMVECTOR normal = XMVector3Normalize(0.5f * (n0 + n1));
	XMVECTOR tangent = XMVector3Normalize(0.5f * (tan0 + tan1));
	XMVECTOR tex = 0.5f * (tex0 + tex1);

	Vertex v;
	XMStoreFloat3(&v.Position, pos);
	XMStoreFloat3(&v.Normal, normal);
	XMStoreFloat3(&v.TangentU, tangent);
	XMStoreFloat2(&v.TexC, tex);
	return v;
}

void GeometryGenerator::BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{

}

void GeometryGenerator::BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{

}
