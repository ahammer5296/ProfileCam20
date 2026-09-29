#include "camera.h"
#include "diagnostics.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <propidl.h>
#include <dvdmedia.h>

namespace {
void FreeMediaType(AM_MEDIA_TYPE* mt) { if (!mt) return; if (mt->pUnk) mt->pUnk->Release(); if (mt->pbFormat) CoTaskMemFree(mt->pbFormat); CoTaskMemFree(mt); }
void ReleaseMediaTypeFields(AM_MEDIA_TYPE& mt) { if(mt.pUnk)mt.pUnk->Release(); if(mt.pbFormat)CoTaskMemFree(mt.pbFormat); mt.pUnk=nullptr;mt.pbFormat=nullptr;mt.cbFormat=0; }
std::wstring ReadFriendlyName(IMoniker* moniker) { IPropertyBag* bag=nullptr; VARIANT value; VariantInit(&value); std::wstring result=L"Camera"; if(SUCCEEDED(moniker->BindToStorage(nullptr,nullptr,IID_PPV_ARGS(&bag)))) { if(SUCCEEDED(bag->Read(L"FriendlyName",&value,nullptr))&&value.vt==VT_BSTR&&value.bstrVal) result=value.bstrVal; bag->Release(); } VariantClear(&value); return result; }
bool ExtractVideoInfo(const AM_MEDIA_TYPE* mt, int& width, int& height, double& fps) { if(!mt||!mt->pbFormat||mt->cbFormat<sizeof(VIDEOINFOHEADER)) return false; if(mt->formattype==FORMAT_VideoInfo) { auto* vi=reinterpret_cast<const VIDEOINFOHEADER*>(mt->pbFormat); width=vi->bmiHeader.biWidth; height=std::abs(vi->bmiHeader.biHeight); fps=vi->AvgTimePerFrame>0?10000000.0/static_cast<double>(vi->AvgTimePerFrame):0.0; } else if(mt->formattype==FORMAT_VideoInfo2&&mt->cbFormat>=sizeof(VIDEOINFOHEADER2)) { auto* vi=reinterpret_cast<const VIDEOINFOHEADER2*>(mt->pbFormat); width=vi->bmiHeader.biWidth; height=std::abs(vi->bmiHeader.biHeight); fps=vi->AvgTimePerFrame>0?10000000.0/static_cast<double>(vi->AvgTimePerFrame):0.0; } else return false; return width>0&&height>0; }
int SubtypeRank(const GUID& subtype) { if(subtype==MEDIASUBTYPE_RGB32)return 4; if(subtype==MEDIASUBTYPE_YUY2)return 3; if(subtype==MEDIASUBTYPE_MJPG)return 2; return 1; }
const wchar_t* SubtypeName(const GUID& subtype) { if(subtype==MEDIASUBTYPE_RGB32)return L"RGB32"; if(subtype==MEDIASUBTYPE_YUY2)return L"YUY2"; if(subtype==MEDIASUBTYPE_MJPG)return L"MJPG"; return L"other"; }
}

static HRESULT BindDeviceFilter(const VideoDevice& device,IBaseFilter** output) {
    if(!output)return E_POINTER;
    *output=nullptr;
    ICreateDevEnum* devEnum=nullptr;
    IEnumMoniker* enumMoniker=nullptr;
    IBindCtx* bindCtx=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_SystemDeviceEnum,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&devEnum));
    if(SUCCEEDED(hr))hr=devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&enumMoniker,0);
    if(hr==S_OK)hr=CreateBindCtx(0,&bindCtx);
    if(SUCCEEDED(hr)&&enumMoniker){
        bool found=false;
        IMoniker* moniker=nullptr;
        ULONG fetched=0;
        while(enumMoniker->Next(1,&moniker,&fetched)==S_OK){
            LPOLESTR name=nullptr;
            if(SUCCEEDED(moniker->GetDisplayName(bindCtx,nullptr,&name))){
                found=device.displayName==name;
                CoTaskMemFree(name);
                if(found)hr=moniker->BindToObject(nullptr,nullptr,IID_IBaseFilter,reinterpret_cast<void**>(output));
            }
            moniker->Release();
            moniker=nullptr;
            if(found)break;
        }
        if(!found)hr=VFW_E_NOT_FOUND;
    }else if(SUCCEEDED(hr))hr=VFW_E_NOT_FOUND;
    if(bindCtx)bindCtx->Release();
    if(enumMoniker)enumMoniker->Release();
    if(devEnum)devEnum->Release();
    return hr;
}

VideoFormat::~VideoFormat(){ FreeMediaType(mediaType); }
VideoFormat::VideoFormat(VideoFormat&& other) noexcept : width(other.width),height(other.height),fps(other.fps),subtype(other.subtype),mediaType(other.mediaType){other.mediaType=nullptr;}
VideoFormat& VideoFormat::operator=(VideoFormat&& other) noexcept { if(this!=&other){FreeMediaType(mediaType); width=other.width;height=other.height;fps=other.fps;subtype=other.subtype;mediaType=other.mediaType;other.mediaType=nullptr;} return *this; }

bool CameraCatalog::EnumerateFormats(VideoDevice& device,IMoniker* moniker) {
    wchar_t startMessage[512]; wsprintfW(startMessage,L"Capability enumeration started: %s",device.friendlyName.c_str()); DiagnosticLog(startMessage);
    if(!moniker){DiagnosticLog(L"Capability enumeration received null moniker");return false;}
    IBaseFilter* filter=nullptr; HRESULT hr=moniker->BindToObject(nullptr,nullptr,IID_IBaseFilter,reinterpret_cast<void**>(&filter));
    if(FAILED(hr)){DiagnosticLogHRESULT(L"Bind camera filter for capabilities",hr);return false;} DiagnosticLog(L"Capability camera filter bound");
    ICaptureGraphBuilder2* builder=nullptr; IGraphBuilder* graph=nullptr; IAMStreamConfig* config=nullptr;
    hr=CoCreateInstance(CLSID_FilterGraph,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&graph));
    if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_CaptureGraphBuilder2,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&builder));
    if(SUCCEEDED(hr))hr=builder->SetFiltergraph(graph);
    if(SUCCEEDED(hr))hr=graph->AddFilter(filter,L"Video Source");
    if(SUCCEEDED(hr))hr=builder->FindInterface(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,filter,IID_IAMStreamConfig,reinterpret_cast<void**>(&config));
    if(FAILED(hr)){DiagnosticLogHRESULT(L"Find IAMStreamConfig",hr);if(config)config->Release();if(builder)builder->Release();if(graph)graph->Release();filter->Release();return false;}
    DiagnosticLog(L"IAMStreamConfig acquired");
    int count=0,size=0; hr=config->GetNumberOfCapabilities(&count,&size);
    if(FAILED(hr)){DiagnosticLogHRESULT(L"GetNumberOfCapabilities",hr);config->Release();builder->Release();graph->Release();filter->Release();return false;}
    wchar_t capabilityMessage[128];wsprintfW(capabilityMessage,L"GetNumberOfCapabilities succeeded: %d entries, block %d bytes",count,size);DiagnosticLog(capabilityMessage);
    std::map<std::pair<int,int>,VideoFormat> best; std::vector<BYTE> caps(static_cast<size_t>(std::max(size,static_cast<int>(sizeof(VIDEO_STREAM_CONFIG_CAPS)))));
    for(int i=0;i<count;i++){
        AM_MEDIA_TYPE* mt=nullptr;hr=config->GetStreamCaps(i,&mt,caps.data());
        if(FAILED(hr)){DiagnosticLogHRESULT(L"GetStreamCaps",hr);continue;}
        int w=0,h=0;double fps=0;
        if(!ExtractVideoInfo(mt,w,h,fps)){DiagnosticLog(L"GetStreamCaps returned unsupported media format");FreeMediaType(mt);continue;}
        if(w>=960&&h>=720){
            double maxFps=0;
            if(size>=static_cast<int>(sizeof(VIDEO_STREAM_CONFIG_CAPS))){
                const auto* streamCaps=reinterpret_cast<const VIDEO_STREAM_CONFIG_CAPS*>(caps.data());
                if(streamCaps->MinFrameInterval>0)maxFps=10000000.0/static_cast<double>(streamCaps->MinFrameInterval);
            }
            wchar_t modeMessage[200];swprintf_s(modeMessage,L"Advertised mode: %dx%d %s %.1f fps (max %.1f)",w,h,SubtypeName(mt->subtype),fps,maxFps);DiagnosticLog(modeMessage);
        }
        VideoFormat candidate;candidate.width=w;candidate.height=h;candidate.fps=fps;candidate.subtype=mt->subtype;candidate.mediaType=mt;
        auto key=std::make_pair(w,h);auto it=best.find(key);
        if(it==best.end())best.emplace(key,std::move(candidate));
        else{
            bool better=candidate.fps>it->second.fps+0.01||
                (std::abs(candidate.fps-it->second.fps)<=0.01&&SubtypeRank(candidate.subtype)>SubtypeRank(it->second.subtype));
            if(better)it->second=std::move(candidate);
        }
    }
    for(auto& item:best)device.formats.push_back(std::move(item.second));std::sort(device.formats.begin(),device.formats.end(),[](const VideoFormat& a,const VideoFormat& b){long long ap=static_cast<long long>(a.width)*a.height,bp=static_cast<long long>(b.width)*b.height;return ap!=bp?ap>bp:(a.width!=b.width?a.width>b.width:a.height>b.height);});
    config->Release();builder->Release();graph->Release();filter->Release();wchar_t summary[256];wsprintfW(summary,L"Capabilities: %s -> %u modes",device.friendlyName.c_str(),static_cast<unsigned>(device.formats.size()));DiagnosticLog(summary);return !device.formats.empty();
}
bool CameraCatalog::Refresh() {
    DiagnosticLog(L"DirectShow device enumeration started"); devices_.clear(); ICreateDevEnum* devEnum=nullptr; IEnumMoniker* enumMoniker=nullptr; HRESULT hr=CoCreateInstance(CLSID_SystemDeviceEnum,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&devEnum)); if(FAILED(hr)){DiagnosticLogHRESULT(L"CoCreate System Device Enumerator",hr);return false;} hr=devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&enumMoniker,0); if(hr!=S_OK){DiagnosticLogHRESULT(L"Create video input class enumerator",hr);devEnum->Release();return false;}
    IBindCtx* bindCtx=nullptr; if(FAILED(CreateBindCtx(0,&bindCtx))){enumMoniker->Release();devEnum->Release();return false;} IMoniker* moniker=nullptr; ULONG fetched=0; while(enumMoniker->Next(1,&moniker,&fetched)==S_OK){LPOLESTR name=nullptr; VideoDevice d; d.friendlyName=ReadFriendlyName(moniker); if(SUCCEEDED(moniker->GetDisplayName(bindCtx,nullptr,&name))){d.displayName=name;CoTaskMemFree(name); EnumerateFormats(d,moniker); if(!d.displayName.empty())devices_.push_back(std::move(d));} moniker->Release();} bindCtx->Release();enumMoniker->Release();devEnum->Release();wchar_t summary[128];wsprintfW(summary,L"DirectShow enumeration complete: %u devices",static_cast<unsigned>(devices_.size()));DiagnosticLog(summary);return !devices_.empty();
}

class CameraCapture::Callback final : public ISampleGrabberCB {
public:
    explicit Callback(CameraCapture* owner):owner_(owner){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override { if(!object)return E_POINTER; *object=nullptr; if(iid==IID_IUnknown||iid==__uuidof(ISampleGrabberCB)){*object=static_cast<ISampleGrabberCB*>(this);AddRef();return S_OK;} return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG value=--refs_; if(!value)delete this; return value; }
    HRESULT STDMETHODCALLTYPE SampleCB(double,IMediaSample*) override { DiagnosticLog(L"Unexpected SampleCB invocation"); return S_OK; }
    HRESULT STDMETHODCALLTYPE BufferCB(double,BYTE* buffer,long length) override { if(owner_)owner_->OnBuffer(buffer,length); return S_OK; }
private: std::atomic<ULONG> refs_{1}; CameraCapture* owner_{};
};

CameraCapture::~CameraCapture(){Stop();}
bool CameraCapture::Fail(const wchar_t* operation,HRESULT hr){wchar_t message[192];wsprintfW(message,L"%s (0x%08lX)",operation,static_cast<unsigned long>(hr));lastError_=message;DiagnosticLogHRESULT(operation,hr);ClearGraph();return false;}
void CameraCapture::ClearGraph(){
    if(graph_)DiagnosticLog(L"Camera graph stopping");
    running_.store(false);
    if(control_)control_->Stop();
    if(graph_)DiagnosticLog(L"Camera graph stopped");
    if(grabber_)grabber_->SetCallback(nullptr,1);
    if(graph_)DiagnosticLog(L"Camera callback detached");
    if(callback_){DiagnosticLog(L"Releasing callback");callback_->Release();callback_=nullptr;}
    if(control_){DiagnosticLog(L"Releasing graph control");control_->Release();control_=nullptr;}
    if(grabber_){DiagnosticLog(L"Releasing sample grabber interface");grabber_->Release();grabber_=nullptr;}
    if(nullRenderer_){DiagnosticLog(L"Releasing null renderer");nullRenderer_->Release();nullRenderer_=nullptr;}
    if(grabberFilter_){DiagnosticLog(L"Releasing sample grabber filter");grabberFilter_->Release();grabberFilter_=nullptr;}
    if(source_){DiagnosticLog(L"Releasing camera source");source_->Release();source_=nullptr;}
    if(builder_){DiagnosticLog(L"Releasing capture graph builder");builder_->Release();builder_=nullptr;}
    if(graph_){DiagnosticLog(L"Releasing filter graph");graph_->Release();graph_=nullptr;DiagnosticLog(L"Camera graph released");}
    notifyWindow_=nullptr;running_=false;notifyPending_.store(false);
}
void CameraCapture::Stop(){ClearGraph();}

bool CameraCapture::Start(const VideoDevice& device,const VideoFormat& format,HWND notifyWindow){
    Stop(); lastError_.clear(); notifyWindow_=notifyWindow; wchar_t startMessage[512];swprintf_s(startMessage,L"Preview start: %s %dx%d %s %.1f fps",device.friendlyName.c_str(),format.width,format.height,SubtypeName(format.subtype),format.fps);DiagnosticLog(startMessage); HRESULT hr=BindDeviceFilter(device,&source_); if(FAILED(hr))return Fail(L"Bind camera",hr); DiagnosticLog(L"Camera filter bound");
    hr=CoCreateInstance(CLSID_FilterGraph,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&graph_)); if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_CaptureGraphBuilder2,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&builder_)); if(SUCCEEDED(hr))hr=builder_->SetFiltergraph(graph_); if(SUCCEEDED(hr))hr=graph_->AddFilter(source_,L"Video Source"); if(FAILED(hr))return Fail(L"Create graph",hr); DiagnosticLog(L"Graph and source filter created");
    hr=CoCreateInstance(CLSID_SampleGrabber,nullptr,CLSCTX_INPROC_SERVER,IID_IBaseFilter,reinterpret_cast<void**>(&grabberFilter_)); if(SUCCEEDED(hr))hr=grabberFilter_->QueryInterface(__uuidof(ISampleGrabber),reinterpret_cast<void**>(&grabber_)); if(SUCCEEDED(hr)){AM_MEDIA_TYPE request{};request.majortype=MEDIATYPE_Video;request.subtype=MEDIASUBTYPE_RGB32;request.formattype=FORMAT_VideoInfo;hr=grabber_->SetMediaType(&request);} if(SUCCEEDED(hr))hr=graph_->AddFilter(grabberFilter_,L"Sample Grabber"); if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_NullRenderer,nullptr,CLSCTX_INPROC_SERVER,IID_IBaseFilter,reinterpret_cast<void**>(&nullRenderer_)); if(SUCCEEDED(hr))hr=graph_->AddFilter(nullRenderer_,L"Null Renderer"); if(FAILED(hr))return Fail(L"Create sample graph",hr); DiagnosticLog(L"Sample Grabber and Null Renderer created");
    IAMStreamConfig* config=nullptr; hr=builder_->FindInterface(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,source_,IID_IAMStreamConfig,reinterpret_cast<void**>(&config));
    if(FAILED(hr))return Fail(L"Find capture format control",hr);
    hr=config->SetFormat(format.mediaType);config->Release();
    if(FAILED(hr))return Fail(L"Set capture format",hr);
    DiagnosticLog(L"Capture format selected");
    callback_=new Callback(this); hr=grabber_->SetBufferSamples(FALSE);if(SUCCEEDED(hr))hr=grabber_->SetOneShot(FALSE);if(SUCCEEDED(hr))hr=grabber_->SetCallback(callback_,1); if(FAILED(hr))return Fail(L"Set buffer callback",hr); DiagnosticLog(L"BufferCB selected (method 1)");
    hr=builder_->RenderStream(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,source_,grabberFilter_,nullRenderer_);
    if(SUCCEEDED(hr))DiagnosticLog(L"Camera stream connected through capture pin");
    else{
        DiagnosticLogHRESULT(L"Connect capture pin",hr);
        hr=builder_->RenderStream(&PIN_CATEGORY_PREVIEW,&MEDIATYPE_Video,source_,grabberFilter_,nullRenderer_);
        if(FAILED(hr))return Fail(L"Connect camera stream",hr);
        DiagnosticLog(L"Camera stream connected through preview pin");
    }
    AM_MEDIA_TYPE connected{};hr=grabber_->GetConnectedMediaType(&connected);if(FAILED(hr))return Fail(L"Read connected format",hr);
    int connectedWidth=0,connectedHeight=0;double connectedFps=0;
    bool valid=connected.subtype==MEDIASUBTYPE_RGB32&&ExtractVideoInfo(&connected,connectedWidth,connectedHeight,connectedFps);
    LONG signedHeight=0;if(valid){if(connected.formattype==FORMAT_VideoInfo)signedHeight=reinterpret_cast<const VIDEOINFOHEADER*>(connected.pbFormat)->bmiHeader.biHeight;else signedHeight=reinterpret_cast<const VIDEOINFOHEADER2*>(connected.pbFormat)->bmiHeader.biHeight;}
    wchar_t connectedMessage[192];wsprintfW(connectedMessage,L"Connected RGB32: %dx%d signedHeight=%ld fps=%d",connectedWidth,connectedHeight,signedHeight,static_cast<int>(connectedFps+0.5));DiagnosticLog(connectedMessage);
    ReleaseMediaTypeFields(connected);
    if(!valid||connectedWidth>16384||connectedHeight>16384)return Fail(L"Unsupported connected format",VFW_E_INVALIDMEDIATYPE);
    {std::lock_guard<std::mutex> lock(frameMutex_);latest_.width=connectedWidth;latest_.height=connectedHeight;latest_.stride=connectedWidth*4;latest_.topDown=signedHeight<0;latest_.pixels.clear();}
    sampleCount_.store(0);
    hr=graph_->QueryInterface(IID_PPV_ARGS(&control_));if(FAILED(hr))return Fail(L"Get graph control",hr);
    running_.store(true);hr=control_->Run(); if(FAILED(hr))return Fail(L"Run camera graph",hr); DiagnosticLog(hr==S_FALSE?L"Camera graph start pending":L"Camera graph running");return true;
}
void CameraCapture::OnBuffer(BYTE* source,long size){if(!source||!running_)return;std::lock_guard<std::mutex> lock(frameMutex_);const size_t expected=static_cast<size_t>(latest_.stride)*latest_.height;unsigned sampleNumber=sampleCount_.fetch_add(1);if(sampleNumber==0||sampleNumber%30==29){wchar_t sampleMessage[160];wsprintfW(sampleMessage,L"Buffer %u received: buffer=%ld expected=%u",sampleNumber+1,size,static_cast<unsigned>(expected));DiagnosticLog(sampleMessage);}if(size<static_cast<long>(expected))return;latest_.pixels.assign(source,source+expected);if(notifyWindow_&&!notifyPending_.exchange(true))PostMessageW(notifyWindow_,WM_APP+20,0,0);}
bool CameraCapture::Snapshot(VideoFrame& frame){std::lock_guard<std::mutex> lock(frameMutex_);if(latest_.pixels.empty())return false;frame=latest_;return true;}
