		virtual void* getNativeHandle() = 0;
		virtual void* getNativeRendererHandle() = 0;
		virtual void* getNativeDeviceContext() = 0;

		virtual bool createVertexBuffer(uint32_t size_in_bytes, IBuffer** output) = 0;
		virtual bool createIndexBuffer(uint32_t size_in_bytes, IBuffer** output) = 0;
		virtual bool createConstantBuffer(uint32_t size_in_bytes, IBuffer** output) = 0;
