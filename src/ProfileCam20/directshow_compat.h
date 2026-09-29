#pragma once
#include <windows.h>
#include <dshow.h>

// qedit.h is absent from current SDKs, so the deprecated Sample Grabber interfaces live here.
MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85") ISampleGrabberCB : public IUnknown { virtual HRESULT STDMETHODCALLTYPE SampleCB(double sampleTime, IMediaSample* sample)=0; virtual HRESULT STDMETHODCALLTYPE BufferCB(double sampleTime, BYTE* buffer, long length)=0; };
MIDL_INTERFACE("6B652FFF-11FE-4FCE-92AD-0266B5D7C78F") ISampleGrabber : public IUnknown { virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL oneShot)=0; virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* type)=0; virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE* type)=0; virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL bufferSamples)=0; virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* size, long* buffer)=0; virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample** sample)=0; virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB* callback, long method)=0; };
constexpr CLSID CLSID_SampleGrabber = {0xC1F400A0, 0x3F08, 0x11D3, {0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37}};
constexpr CLSID CLSID_NullRenderer = {0xC1F400A4, 0x3F08, 0x11D3, {0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37}};
