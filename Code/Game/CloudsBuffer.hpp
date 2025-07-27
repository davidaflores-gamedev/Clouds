#pragma once

#include <d3d11.h>

class CloudManager;

class CloudsBuffer
{
	friend class Renderer;
	
public:
	CloudsBuffer(ID3D11Device * device, size_t size, CloudManager* manager);

	CloudsBuffer(const CloudsBuffer& copy) = delete;
	virtual ~CloudsBuffer();

	void Create();
	void Resize(size_t size);

	size_t GetSize() const { return m_size; }

public:
	ID3D11Device* m_device = nullptr;
	ID3D11Buffer* m_buffer = nullptr;
	size_t m_size = 0;
	ID3D11ShaderResourceView* CloudsSRV = nullptr;
	CloudManager* m_manager = nullptr;
};	

