#include "DiligentGraphicsAdapterGL.h"

#if GL_SUPPORTED || GLES_SUPPORTED

#include "UnityGraphicsGLCoreES_Emulator.h"
#include "EngineFactoryOpenGL.h"
#include "SwapChainBase.h"
#include "DefaultRawMemoryAllocator.h"

#include "UnityGraphicsGL_Impl.h"
#include "SwapChainGL.h"
#include "RenderDeviceGL.h"
#include "DeviceContextGL.h"

using namespace Diligent;

namespace
{

class ProxySwapChainGL : public SwapChainBase<ISwapChainGL>
{
public:
    using TBase = SwapChainBase<ISwapChainGL>;

    ProxySwapChainGL( IReferenceCounters *pRefCounters,
                      const UnityGraphicsGLCoreES_Emulator& UnityGraphicsGL,
                      IRenderDevice *pDevice,
                      IDeviceContext *pDeviceContext,
                      const SwapChainDesc& SCDesc ) : 
        TBase(pRefCounters, pDevice, pDeviceContext,SCDesc),
        m_UnityGraphicsGL(UnityGraphicsGL)
    {
        TextureDesc DummyTexDesc;
        DummyTexDesc.Name      = "Back buffer proxy";
        DummyTexDesc.Type      = RESOURCE_DIM_TEX_2D;
        DummyTexDesc.Format    = SCDesc.ColorBufferFormat;
        DummyTexDesc.Width     = SCDesc.Width;
        DummyTexDesc.Height    = SCDesc.Height;
        DummyTexDesc.BindFlags = BIND_RENDER_TARGET;
        RefCntAutoPtr<IRenderDeviceGL> pDeviceGL(pDevice, IID_RenderDeviceGL);
        RefCntAutoPtr<ITexture> pDummyRenderTarget;
        pDeviceGL->CreateDummyTexture(DummyTexDesc, RESOURCE_STATE_RENDER_TARGET, &pDummyRenderTarget);
        m_pRTV = pDummyRenderTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

        DummyTexDesc.Name      = "Depth buffer proxy";
        DummyTexDesc.Format    = SCDesc.DepthBufferFormat;
        DummyTexDesc.BindFlags = BIND_DEPTH_STENCIL;
        RefCntAutoPtr<ITexture> pDummyDepthBuffer;
        pDeviceGL->CreateDummyTexture(DummyTexDesc, RESOURCE_STATE_DEPTH_WRITE, &pDummyDepthBuffer);
        m_pDSV = pDummyDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    }
        
    virtual void Present(Uint32 SyncInterval)override final
    {
        UNEXPECTED("Present is not expected to be called directly");
    }

    virtual void SetFullscreenMode(const DisplayModeAttribs &DisplayMode)override final
    {
        UNEXPECTED("Fullscreen mode cannot be set through the proxy swap chain");
    }

    virtual void SetWindowedMode()override final
    {
        UNEXPECTED("Windowed mode cannot be set through the proxy swap chain");
    }

    virtual void Resize(Uint32 NewWidth, Uint32 NewHeight)override final
    {
        TBase::Resize(NewWidth, NewHeight, 0);
    }

    virtual GLuint GetDefaultFBO()const override final
    {
        return m_UnityGraphicsGL.GetGraphicsImpl()->GetDefaultFBO();
    }
    
    virtual ITextureView* GetCurrentBackBufferRTV()override final{return m_pRTV;}
    virtual ITextureView* GetDepthBufferDSV()override final{return m_pDSV;}

private:
    const UnityGraphicsGLCoreES_Emulator& m_UnityGraphicsGL;
    RefCntAutoPtr<ITextureView> m_pRTV;
    RefCntAutoPtr<ITextureView> m_pDSV;
};

}

DiligentGraphicsAdapterGL::DiligentGraphicsAdapterGL(const UnityGraphicsGLCoreES_Emulator& UnityGraphicsGL)noexcept :
    m_UnityGraphicsGL(UnityGraphicsGL)
{
    auto *UnityGraphicsGLImpl = UnityGraphicsGL.GetGraphicsImpl();

    auto *pFactoryGL = GetEngineFactoryOpenGL();
    EngineGLCreateInfo Attribs;
    pFactoryGL->AttachToActiveGLContext(Attribs, &m_pDevice, &m_pDeviceCtx);

    auto BackBufferGLFormat = UnityGraphicsGLImpl->GetBackBufferFormat();
    auto DepthBufferGLFormat = UnityGraphicsGLImpl->GetDepthBufferFormat();

    SwapChainDesc SCDesc;
    if(BackBufferGLFormat == GL_RGBA8)
        SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    else
    {
        UNEXPECTED("Unexpected back buffer format");
    }

    if (DepthBufferGLFormat == GL_DEPTH_COMPONENT32F)
        SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    else if (DepthBufferGLFormat == GL_DEPTH_COMPONENT24)
        SCDesc.DepthBufferFormat = TEX_FORMAT_D24_UNORM_S8_UINT;
    else if (DepthBufferGLFormat == GL_DEPTH_COMPONENT16)
        SCDesc.DepthBufferFormat = TEX_FORMAT_D16_UNORM;
    else
    {
        UNEXPECTED("Unexpected depth buffer format");
    }

    SCDesc.Width = UnityGraphicsGLImpl->GetBackBufferWidth();
    SCDesc.Height = UnityGraphicsGLImpl->GetBackBufferHeight();
    // This field is irrelevant
    SCDesc.BufferCount = 0;

    auto &DefaultAllocator = DefaultRawMemoryAllocator::GetAllocator();
    auto pProxySwapChainGL = NEW_RC_OBJ(DefaultAllocator, "ProxySwapChainGL instance", ProxySwapChainGL)(m_UnityGraphicsGL, m_pDevice, m_pDeviceCtx, SCDesc);
    pProxySwapChainGL->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(static_cast<ISwapChain**>(&m_pProxySwapChain)));

    RefCntAutoPtr<IDeviceContextGL>(m_pDeviceCtx, IID_DeviceContextGL)->SetSwapChain(pProxySwapChainGL);
}

void DiligentGraphicsAdapterGL::BeginFrame()
{
    auto *UnityGraphicsGLImpl = m_UnityGraphicsGL.GetGraphicsImpl();
    Uint32 Width = UnityGraphicsGLImpl->GetBackBufferWidth();
    Uint32 Height = UnityGraphicsGLImpl->GetBackBufferHeight();
    m_pProxySwapChain.RawPtr<ProxySwapChainGL>()->Resize(Width, Height);
}
    
void DiligentGraphicsAdapterGL::EndFrame()
{
    m_pDeviceCtx->InvalidateState();
}

bool DiligentGraphicsAdapterGL::UsesReverseZ()
{ 
    return m_UnityGraphicsGL.UsesReverseZ(); 
}

#endif // GL_SUPPORTED || GLES_SUPPORTED
