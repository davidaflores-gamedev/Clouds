#include "CloudsBuffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/CloudManager.hpp"

CloudsBuffer::CloudsBuffer(ID3D11Device* device, size_t size, CloudManager* manager)
	:m_device(device)
	, m_size(size)
	, m_manager(manager)

{
	Create();
}

CloudsBuffer::~CloudsBuffer()
{
	SafeRelease(m_buffer);
	SafeRelease(CloudsSRV);
}

void CloudsBuffer::Create()
{
	// Create a buffer for the detailed Clouds data
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(Cloud) * (UINT)m_size;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(Cloud);
	bufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA voxelData = {};
	voxelData.pSysMem = m_manager->m_clouds.data();

	m_device->CreateBuffer(&bufferDesc, &voxelData, &m_buffer);

	// Create a Shader Resource View (SRV) for the Clouds buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC CloudsSRVDesc = {};
	CloudsSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	CloudsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	CloudsSRVDesc.Buffer.ElementWidth = sizeof(Cloud);

	m_device->CreateShaderResourceView(m_buffer, &CloudsSRVDesc, &CloudsSRV);
}

void CloudsBuffer::Resize(size_t size)
{
	SafeRelease(m_buffer);

	m_size = size;

	Create();

}