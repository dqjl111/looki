#include "lwindowsglhelper.h"
#include "lkcore/plugin/ldynamiclibrary.h"
#include <d3d9.h>

LK_NAMESPACE_BEGIN

static const DWORD VENDOR_ID_AMD = 0x1002;

static std::shared_ptr<LGpuDescription> adapterIdentifierToGpuDescription(const D3DADAPTER_IDENTIFIER9 &adapterIdentifier)
{
    std::shared_ptr<LGpuDescription> result = std::make_shared<LGpuDescription>();
    result->m_vendorId = adapterIdentifier.VendorId;
    result->m_deviceId = adapterIdentifier.DeviceId;
    result->m_revision = adapterIdentifier.Revision;
    result->m_subSysId = adapterIdentifier.SubSysId;
    std::vector<uint32_t>& version = result->m_driverVersion;
    version.resize(4);
    version[0] = HIWORD(adapterIdentifier.DriverVersion.HighPart); // Product
    version[1] = LOWORD(adapterIdentifier.DriverVersion.HighPart); // Version
    version[2] = HIWORD(adapterIdentifier.DriverVersion.LowPart); // Sub version
    version[3] = LOWORD(adapterIdentifier.DriverVersion.LowPart); // build
    result->m_driverName = adapterIdentifier.Driver;
    result->m_description = adapterIdentifier.Description;
    return result;
}

class LDirect3D9Handle
{
public:
    LDirect3D9Handle();
    ~LDirect3D9Handle();

    bool isValid() const { return m_direct3D9 != nullptr; }

    UINT adapterCount() const { return m_direct3D9 ? m_direct3D9->GetAdapterCount() : 0u; }
    bool retrieveAdapterIdentifier(UINT n, D3DADAPTER_IDENTIFIER9 *adapterIdentifier) const;

private:
    std::shared_ptr<LDynamicLibrary> m_d3d9lib = nullptr;
    IDirect3D9 *m_direct3D9 = nullptr;
};

LDirect3D9Handle::LDirect3D9Handle()
{
    m_d3d9lib = std::make_shared<LDynamicLibrary>("d3d9", LDynamicLibrary::SystemPath);
    using PtrDirect3DCreate9 = IDirect3D9 *(WINAPI *)(UINT);
    if (auto direct3DCreate9 = (PtrDirect3DCreate9)m_d3d9lib->resolve("Direct3DCreate9")){
        m_direct3D9 = direct3DCreate9(D3D_SDK_VERSION);
    }
}

LDirect3D9Handle::~LDirect3D9Handle()
{
    if (m_direct3D9){
        m_direct3D9->Release();
    }
}

bool LDirect3D9Handle::retrieveAdapterIdentifier(UINT n, D3DADAPTER_IDENTIFIER9 *adapterIdentifier) const
{
    return m_direct3D9
           && SUCCEEDED(m_direct3D9->GetAdapterIdentifier(n, 0, adapterIdentifier));
}


LGpuDescription::LGpuDescription()
{

}

std::shared_ptr<LGpuDescription> LGpuDescription::detect()
{
    std::shared_ptr<LGpuDescription> result = std::make_shared<LGpuDescription>();
    LDirect3D9Handle direct3D9;
    if (!direct3D9.isValid()){
        return result;
    }

    D3DADAPTER_IDENTIFIER9 adapterIdentifier;
    bool isAMD = false;
    // Adapter "0" is D3DADAPTER_DEFAULT which returns the default adapter. In
    // multi-GPU, multi-screen setups this is the GPU that is associated with
    // the "main display" in the Display Settings, and this is the GPU OpenGL
    // and D3D uses by default. Therefore querying any additional adapters is
    // futile and not useful for our purposes in general, except for
    // identifying a few special cases later on.
    if (direct3D9.retrieveAdapterIdentifier(0, &adapterIdentifier)) {
        result = adapterIdentifierToGpuDescription(adapterIdentifier);
        isAMD = result->m_vendorId == VENDOR_ID_AMD;
    }

    // Having AMD as the default adapter results in a crash
    // when starting apps on a screen connected to the Intel card by looking
    // for a default AMD adapter and an additional non-AMD one.
    if (isAMD) {
        const UINT adapterCount = direct3D9.adapterCount();
        for (UINT adp = 1; adp < adapterCount; ++adp) {
            if (direct3D9.retrieveAdapterIdentifier(adp, &adapterIdentifier)
                && adapterIdentifier.VendorId != VENDOR_ID_AMD) {
                // Bingo. Now figure out the display for the AMD card.
                DISPLAY_DEVICEA dd;
                memset(&dd, 0, sizeof(dd));
                dd.cb = sizeof(dd);
                for (int dev = 0; EnumDisplayDevicesA(nullptr, dev, &dd, 0); ++dev) {
                    if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                        // DeviceName is something like \\.\DISPLAY1 which can be used to
                        // match with the MONITORINFOEX::szDevice queried by WindowsScreen.
                        result->m_gpuSuitableScreen = dd.DeviceName;
                        break;
                    }
                }
                break;
            }
        }
    }

    return result;

}

std::vector<std::shared_ptr<LGpuDescription> > LGpuDescription::detectAll()
{
    std::vector<std::shared_ptr<LGpuDescription> > result;
    LDirect3D9Handle direct3D9;
    if (const UINT adapterCount = direct3D9.adapterCount()) {
        for (UINT adp = 0; adp < adapterCount; ++adp) {
            D3DADAPTER_IDENTIFIER9 adapterIdentifier;
            if (direct3D9.retrieveAdapterIdentifier(adp, &adapterIdentifier))
                result.push_back(adapterIdentifierToGpuDescription(adapterIdentifier));
        }
    }
    return result;
}



//LWindowsGLHelper::LWindowsGLHelper()
//{

//}


LK_NAMESPACE_END
