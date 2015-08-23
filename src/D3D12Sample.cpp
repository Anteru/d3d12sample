#include "D3D12Sample.h"

#include <dxgi1_4.h>
#include <d3dx12.h>
#include <iostream>
#include <d3dcompiler.h>
#include <shaders.h>
#include <sample_texture.h>
#include <algorithm>

#include "ImageIO.h"
#include "Window.h"

#ifdef max 
#undef max
#endif

using namespace Microsoft::WRL;

namespace anteru {
namespace {
struct RenderEnvironment
{
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<IDXGIFactory4> dxgiFactory;
	ComPtr<IDXGISwapChain> swapChain;
};

///////////////////////////////////////////////////////////////////////////////
RenderEnvironment CreateDeviceAndSwapChain (
	_In_opt_ IDXGIAdapter* adapter,
	D3D_FEATURE_LEVEL minimumFeatureLevel,
	_In_opt_ const DXGI_SWAP_CHAIN_DESC* swapChainDesc)
{
	RenderEnvironment result;

	auto hr = D3D12CreateDevice (adapter, minimumFeatureLevel,
		IID_PPV_ARGS (&result.device));

	if (FAILED (hr)) {
		throw std::runtime_error ("Device creation failed.");
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = result.device->CreateCommandQueue (&queueDesc, IID_PPV_ARGS (&result.queue));

	if (FAILED (hr)) {
		throw std::runtime_error ("Command queue creation failed.");
	}

	hr = CreateDXGIFactory1 (IID_PPV_ARGS (&result.dxgiFactory));

	if (FAILED (hr)) {
		throw std::runtime_error ("DXGI factory creation failed.");
	}

	// Must copy into non-const space
	DXGI_SWAP_CHAIN_DESC swapChainDescCopy = *swapChainDesc;
	hr = result.dxgiFactory->CreateSwapChain (
		result.queue.Get (),
		&swapChainDescCopy,
		&result.swapChain
		);

	if (FAILED (hr)) {
		throw std::runtime_error ("Swap chain creation failed.");
	}

	return result;
}
}

///////////////////////////////////////////////////////////////////////////////
D3D12Sample::D3D12Sample ()
{
}

///////////////////////////////////////////////////////////////////////////////
D3D12Sample::~D3D12Sample ()
{
}

void D3D12Sample::Render ()
{
	ID3D12PipelineState* initialState = nullptr;

	{
		commandAllocators_ [currentBackBuffer_]->Reset ();

		auto commandList = commandLists_ [currentBackBuffer_].Get ();
		commandList->Reset (
			commandAllocators_ [currentBackBuffer_].Get (), initialState);
		commandList->OMSetRenderTargets (1, &descriptorHeap_->GetCPUDescriptorHandleForHeapStart (), true, nullptr);
		commandList->RSSetViewports (1, &viewport_);
		commandList->RSSetScissorRects (1, &rectScissor_);

		// Transition back buffer
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Transition.pResource = renderTarget_.Get ();
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier (1, &barrier);

		static const float clearColor [] = {
			0.042f, 0.042f, 0.042f,
			1
		};

		commandList->ClearRenderTargetView (descriptorHeap_->GetCPUDescriptorHandleForHeapStart (),
			clearColor, 0, nullptr);
	}

	static int frameNumber = 0;
	frameNumber++;

	{
		auto commandList = commandLists_ [currentBackBuffer_].Get ();

		void* p;
		constantBuffers_ [GetQueueSlot ()]->Map (0, nullptr, &p);
		float* f = static_cast<float*>(p);
		f [0] = std::abs (std::sin (static_cast<float> (frameNumber) / 64.0f));
		constantBuffers_ [GetQueueSlot ()]->Unmap (0, nullptr);
		
		ID3D12DescriptorHeap* heaps [] = { srvDescriptorHeap_.Get () };
		commandList->SetPipelineState (pso_.Get ());
		commandList->SetGraphicsRootSignature (rootSignature_.Get ());
		commandList->SetDescriptorHeaps (1, heaps);
		commandList->SetGraphicsRootConstantBufferView (1, 
			constantBuffers_ [GetQueueSlot ()]->GetGPUVirtualAddress ());
		commandList->SetGraphicsRootDescriptorTable (0,
			srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart ());

		commandList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers (0, 1, &vertexBufferView_);
		commandList->IASetIndexBuffer (&indexBufferView_);
		commandList->DrawIndexedInstanced (6, 1, 0, 0, 0);
	}

	{
		// Transition the swap chain back to present
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Transition.pResource = renderTarget_.Get ();
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		auto commandList = commandLists_ [currentBackBuffer_].Get ();
		commandList->ResourceBarrier (1, &barrier);

		commandList->Close ();

		ID3D12CommandList* commandLists [] = { commandList };
		commandQueue_->ExecuteCommandLists (std::extent<decltype(commandLists)>::value, commandLists);
	}
}

namespace {
void WaitForFence (ID3D12Fence* fence, UINT64 completionValue, HANDLE waitEvent)
{
	if (fence->GetCompletedValue () < completionValue) {
		fence->SetEventOnCompletion (completionValue, waitEvent);
		WaitForSingleObject (waitEvent, INFINITE);
	}
}
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Run (const int frameCount)
{
	Initialize ();

	for (int i = 0; i < frameCount; ++i) {
		WaitForFence (frameFences_[GetQueueSlot ()].Get (), 
			fenceValues_[GetQueueSlot ()], frameFenceEvents_[GetQueueSlot ()]);
		// We can readback the queries for the current slot
		
		Render ();
		Present ();
	}

	// Drain the queue, wait for everything to finish
	for (int i = 0; i < GetQueueSlotCount (); ++i) {
		WaitForFence (frameFences_[i].Get (), fenceValues_[i], frameFenceEvents_[i]);
	}

	Shutdown ();
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Present ()
{
	swapChain_->Present (1, 0);
	currentBackBuffer_ = (currentBackBuffer_ + 1) % GetQueueSlotCount ();
	swapChain_->GetBuffer (currentBackBuffer_, IID_PPV_ARGS (&renderTarget_));
	device_->CreateRenderTargetView (renderTarget_.Get (), nullptr,
		descriptorHeap_->GetCPUDescriptorHandleForHeapStart ());

	const auto fenceValue = currentFenceValue_;
	commandQueue_->Signal (frameFences_ [currentBackBuffer_].Get (), fenceValue);
	fenceValues_[currentBackBuffer_] = fenceValue;
	++currentFenceValue_;
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Initialize ()
{
	window_.reset (new Window ("Anteru's D3D12 sample", 512, 512));

	CreateDevice ();
	CreateRootSignature ();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device_->CreateDescriptorHeap (&heapDesc, IID_PPV_ARGS (&descriptorHeap_));

	currentFenceValue_ = 0;

	for (int i = 0; i < GetQueueSlotCount (); ++i) {
		frameFenceEvents_[i] = CreateEvent (nullptr, FALSE, FALSE, nullptr);
		fenceValues_[i] = 0;
		device_->CreateFence (currentFenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&frameFences_ [i]));
	}

	swapChain_->GetBuffer (currentBackBuffer_, IID_PPV_ARGS (&renderTarget_));
	device_->CreateRenderTargetView (renderTarget_.Get (), nullptr,
		descriptorHeap_->GetCPUDescriptorHandleForHeapStart ());

	static const D3D12_INPUT_ELEMENT_DESC layout [] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	ComPtr<ID3DBlob> vertexShader;
	D3DCompile (SampleShaders, sizeof (SampleShaders),
		"", nullptr, nullptr,
		"VS_main", "vs_5_0", 0, 0, &vertexShader, nullptr);

	ComPtr<ID3DBlob> pixelShader;
	D3DCompile (SampleShaders, sizeof (SampleShaders),
		"", nullptr, nullptr,
		"PS_main", "ps_5_0", 0, 0, &pixelShader, nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.VS.BytecodeLength = vertexShader->GetBufferSize ();
	psoDesc.VS.pShaderBytecode = vertexShader->GetBufferPointer ();
	psoDesc.PS.BytecodeLength = pixelShader->GetBufferSize ();
	psoDesc.PS.pShaderBytecode = pixelShader->GetBufferPointer ();
	psoDesc.pRootSignature = rootSignature_.Get ();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats [0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.InputLayout.NumElements = std::extent<decltype(layout)>::value;
	psoDesc.InputLayout.pInputElementDescs = layout;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
	psoDesc.SampleDesc.Count = 1;
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = false;
	psoDesc.SampleMask = 0xFFFFFFFF;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	device_->CreateGraphicsPipelineState (&psoDesc, IID_PPV_ARGS (&pso_));

	device_->CreateFence (0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&uploadFence_));

	device_->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS (&initCommandAllocator_));
	device_->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		initCommandAllocator_.Get (), nullptr,
		IID_PPV_ARGS (&initCommandList_));

	CreateMeshBuffers ();
	CreateConstantBuffer ();
	CreateTexture ();

	initCommandList_->Close ();

	ID3D12CommandList* commandLists [] = { initCommandList_.Get () };
	commandQueue_->ExecuteCommandLists (std::extent<decltype(commandLists)>::value, commandLists);
	commandQueue_->Signal (uploadFence_.Get (), 1);

	auto waitEvent = CreateEvent (nullptr, FALSE, FALSE, "Upload Fence Event");
	if (uploadFence_->GetCompletedValue () != 1) {
		uploadFence_->SetEventOnCompletion (1, waitEvent);
		WaitForSingleObject (waitEvent, INFINITE);
	}

	initCommandAllocator_->Reset ();
	initCommandList_.Reset ();
	initCommandAllocator_.Reset ();

	CloseHandle (waitEvent);
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Shutdown ()
{
	for (auto event : frameFenceEvents_) {
		CloseHandle (event);
	}
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateDevice ()
{
	ComPtr<ID3D12Debug> debugController;
	D3D12GetDebugInterface (IID_PPV_ARGS (&debugController));
	debugController->EnableDebugLayer ();

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	::ZeroMemory (&swapChainDesc, sizeof (swapChainDesc));

	swapChainDesc.BufferCount = GetQueueSlotCount ();
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferDesc.Width = window_->GetWidth ();
	swapChainDesc.BufferDesc.Height = window_->GetHeight ();
	swapChainDesc.OutputWindow = window_->GetHWND ();
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Windowed = true;

	auto renderEnv = CreateDeviceAndSwapChain (nullptr, D3D_FEATURE_LEVEL_11_0,
		&swapChainDesc);

	device_ = renderEnv.device;
	commandQueue_ = renderEnv.queue;
	swapChain_ = renderEnv.swapChain;

	for (int i = 0; i < GetQueueSlotCount (); ++i) {
		device_->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS (&commandAllocators_ [i]));
		device_->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocators_[i].Get (), nullptr,
			IID_PPV_ARGS (&commandLists_[i]));
		commandLists_[i]->Close ();
	}

	rectScissor_ = { 0, 0, window_->GetWidth (), window_->GetHeight () };

	viewport_ = { 0.0f, 0.0f,
		static_cast<float>(window_->GetWidth ()),
		static_cast<float>(window_->GetHeight ()),
		0.0f, 1.0f
	};
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateRootSignature ()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.NumDescriptors = 1;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NodeMask = 0;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	device_->CreateDescriptorHeap (&descriptorHeapDesc, IID_PPV_ARGS (&srvDescriptorHeap_));

	ComPtr<ID3DBlob> rootBlob;
	ComPtr<ID3DBlob> errorBlob;
	CD3DX12_ROOT_PARAMETER parameters [2];
	CD3DX12_DESCRIPTOR_RANGE range{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 };
	parameters [0].InitAsDescriptorTable (1, &range);
	parameters [1].InitAsConstantBufferView (0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_STATIC_SAMPLER_DESC samplers [1];
	samplers [0].Init (0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT);

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;

	descRootSignature.Init (2, parameters, 1, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	D3D12SerializeRootSignature (&descRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob);
	device_->CreateRootSignature (0,
		rootBlob->GetBufferPointer (),
		rootBlob->GetBufferSize (), IID_PPV_ARGS (&rootSignature_));
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateConstantBuffer ()
{
	struct ConstantBuffer
	{
		float x, y, z, w;
	};

	static const ConstantBuffer cb = { 0, 0, 0, 0 };

	for (int i = 0; i < GetQueueSlotCount (); ++i) {
		// These will remain in upload heap because we use them only once per
		// frame.
		device_->CreateCommittedResource (&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer (sizeof (ConstantBuffer)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS (&constantBuffers_ [i]));

		void* p;
		constantBuffers_ [i]->Map (0, nullptr, &p);
		::memcpy (p, &cb, sizeof (cb));
		constantBuffers_ [i]->Unmap (0, nullptr);
	}
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateMeshBuffers ()
{
	struct Vertex
	{
		float position [3];
		float uv [2];
	};

	static const Vertex vertices [4] = {
		// Upper Left
		{ { -1.0f, 1.0f, 0 },{ 0, 0 } },
		// Upper Right
		{ { 1.0f, 1.0f, 0 },{ 1, 0 } },
		// Bottom right
		{ { 1.0f, -1.0f, 0 },{ 1, 1 } },
		// Bottom left
		{ { -1.0f, -1.0f, 0 },{ 0, 1 } }
	};

	static const int indices [6] = {
		0, 1, 2, 2, 3, 0
	};

	static const int uploadBufferSize = sizeof (vertices) + sizeof (indices);

	// Create upload buffer
	device_->CreateCommittedResource (&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&uploadBuffer_));

	// Create vertex & index buffer
	device_->CreateCommittedResource (&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (sizeof (vertices)),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS (&vertexBuffer_));

	device_->CreateCommittedResource (&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (sizeof (indices)),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS (&indexBuffer_));

	// Create buffer views
	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress ();
	vertexBufferView_.SizeInBytes = sizeof (vertices);
	vertexBufferView_.StrideInBytes = sizeof (Vertex);

	indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress ();
	indexBufferView_.SizeInBytes = sizeof (indices);
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// Upload
	void* p;
	uploadBuffer_->Map (0, nullptr, &p);
	::memcpy (p, vertices, sizeof (vertices));
	::memcpy (static_cast<unsigned char*>(p) + sizeof (vertices), indices, sizeof (indices));
	uploadBuffer_->Unmap (0, nullptr);

	// Copies
	initCommandList_->CopyBufferRegion (vertexBuffer_.Get (), 0,
		uploadBuffer_.Get (), 0, sizeof (vertices));
	initCommandList_->CopyBufferRegion (indexBuffer_.Get (), 0,
		uploadBuffer_.Get (), sizeof (vertices), sizeof (indices));

	// Barriers
	initCommandList_->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (vertexBuffer_.Get (),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	initCommandList_->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (indexBuffer_.Get (),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateTexture ()
{
	int width = 0, height = 0;

	imageData_ = LoadImageFromMemory (SampleTexture, sizeof (SampleTexture),
		1 /* tight row packing */, &width, &height);
	device_->CreateCommittedResource (&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D (DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS (&image_));

	const auto uploadBufferSize = GetRequiredIntermediateSize (image_.Get (), 0, 1);
	device_->CreateCommittedResource (&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&uploadImage_));

	D3D12_SUBRESOURCE_DATA srcData;
	srcData.pData = imageData_.data ();
	srcData.RowPitch = width * 4;
	srcData.SlicePitch = width * height * 4;

	UpdateSubresources (initCommandList_.Get (), image_.Get (), uploadImage_.Get (), 0, 0, 1, &srcData);
	initCommandList_->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (image_.Get (),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
	shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	shaderResourceViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device_->CreateShaderResourceView (image_.Get (), &shaderResourceViewDesc,
		srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart ());
}
}

int main (int argc, char* argv [])
{
	anteru::D3D12Sample sample;
	sample.Run (512);
}