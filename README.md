D3D12 sample
============

Welcome to a tiny D3D12 sample, which shows how to set up a window and render a textured quad complete with proper uploading handling, multiple frames queued and constant buffers.

License
-------

2-clause BSD, see `COPYING` for details.

System requirements
-------------------

* Windows 10, 64-bit
* Visual Studio 2015 with Visual C++ and Windows SDK installed. The free community edition is sufficient.
* A graphics card with D3D12 support, for instance, any GCN based AMD GPU

Building
--------

Use CMake to build the project. After project generation, you'll have to set the target platform version to Windows 10 by right-clicking on `anD3D12Sample`, `General`, and then changing the `Target Platform Version` to `10.0.10240.0` (or later.)

Points of interest
------------------

The actual application is in `src/D3D12Sample.cpp`. The rest is scaffolding of very minor interest; `ImageIO` has some helper classes to load an image from disk using WIC, `Window` contains a class to create a Win32 Window. All D3D12 code lives in `D3D12Sample.cpp` and `D3D12Sample.h`.

* The application queues multiple frames. To protect the per-frame command lists and other resources, fences are created. After the command list for a frame is submitted, the fence is signaled and the next command list is used. Before the wrap-around occures, the application waits for the fence to ensure GPU resources don't get overwritten.
* The texture and mesh data is uploaded using an upload heap. This happens during the initialization and shows how to transfer data to the GPU. Ideally, this should be running on the copy queue but for the sake of simplicity it is run on the general graphics queue.
* Constant buffers are placed in an `upload` heap. For constant buffers which are read exactly once per frame this is preferable to copying the over to the GPU and then reading them once.
* Barriers are as specific as possible and grouped. Transitioning many resources in one barrier is faster than using multiple barriers as the GPU have to flush caches, and if multiple barriers are grouped, the caches are only flushed once.
* The application uses a root signature slot for the most frequently changing constant buffer.
