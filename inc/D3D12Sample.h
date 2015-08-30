#ifndef ANTERU_D3D12_SAMPLE_D3D12TEST_H_
#define ANTERU_D3D12_SAMPLE_D3D12TEST_H_

#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <vector>

namespace anteru {
class Window;

///////////////////////////////////////////////////////////////////////////////
class D3D12Sample
{
private:
public:

	D3D12Sample ();
	~D3D12Sample ();

	void Run (const int frameCount);

protected:
	int GetQueueSlot () const
	{
		return currentBackBuffer_;
	}

	static const int QUEUE_SLOT_COUNT = 3;

	static constexpr int GetQueueSlotCount ()
	{
		return QUEUE_SLOT_COUNT;
	}

	D3D12_VIEWPORT viewport_;
	D3D12_RECT rectScissor_;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTarget_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

	HANDLE frameFenceEvents_ [QUEUE_SLOT_COUNT];
	Microsoft::WRL::ComPtr<ID3D12Fence> frameFences_ [QUEUE_SLOT_COUNT];
	UINT64 currentFenceValue_;
	UINT64 fenceValues_[QUEUE_SLOT_COUNT];

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> renderTargetDescriptorHeap_;

private:
	void Initialize ();
	void Shutdown ();

	void PrepareRender ();
	void FinalizeRender ();

	void Render ();
	void Present ();
	void CreateDeviceAndSwapChain ();
	void CreateAllocatorsAndCommandLists ();
	void CreateViewportScissor ();
	void CreateRootSignature ();
	void CreateMeshBuffers ();
	void CreatePipelineStateObject ();
	void CreateConstantBuffer ();
	void CreateTexture ();
	void SetupSwapChain ();
	void CreateRenderTargetView ();

	std::unique_ptr<Window> window_;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[QUEUE_SLOT_COUNT];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandLists_[QUEUE_SLOT_COUNT];

	int currentBackBuffer_ = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer_;

	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffers_ [QUEUE_SLOT_COUNT];

	Microsoft::WRL::ComPtr<ID3D12Resource> image_;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadImage_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    srvDescriptorHeap_;

	Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence_;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> initCommandList_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> initCommandAllocator_;

	std::vector<std::uint8_t> imageData_;
};
}

#endif