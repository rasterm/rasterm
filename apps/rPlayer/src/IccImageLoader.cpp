/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/IccImageLoader.hpp>

#include <opencv2/imgcodecs.hpp>

#include <windows.h>
#include <wincodec.h>

#include <string>

#pragma comment(lib, "windowscodecs.lib")

namespace rasterm::rPlayer {
namespace {

template<typename T>
void release(T*& value) noexcept
{
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

std::wstring widen(const std::string& value)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(length, L'\0');
    if (length > 0) {
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                            result.data(), length);
    }
    return result;
}

}

cv::Mat loadColorManagedBgr(const std::string& path)
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICColorContext* sourceColor = nullptr;
    IWICColorContext* destinationColor = nullptr;
    IWICColorTransform* transform = nullptr;
    IWICFormatConverter* converter = nullptr;
    IWICBitmapSource* source = nullptr;

    const std::wstring widePath = widen(path);
    HRESULT status = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
    if (SUCCEEDED(status)) {
        status = factory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
    }
    if (SUCCEEDED(status)) status = decoder->GetFrame(0, &frame);

    UINT contextCount = 0;
    if (SUCCEEDED(status)) status = frame->GetColorContexts(0, nullptr, &contextCount);
    if (SUCCEEDED(status) && contextCount > 0) {
        status = factory->CreateColorContext(&sourceColor);
        if (SUCCEEDED(status)) {
            UINT actual = 0;
            status = frame->GetColorContexts(1, &sourceColor, &actual);
            if (SUCCEEDED(status) && actual > 0) status = factory->CreateColorContext(&destinationColor);
            if (SUCCEEDED(status) && actual > 0) status = destinationColor->InitializeFromExifColorSpace(1);
            if (SUCCEEDED(status) && actual > 0) status = factory->CreateColorTransformer(&transform);
            if (SUCCEEDED(status) && actual > 0) {
                status = transform->Initialize(frame, sourceColor, destinationColor,
                                               GUID_WICPixelFormat24bppBGR);
                source = transform;
            }
        }
    }
    if (source == nullptr) {
        status = S_OK;
        status = factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(status)) {
            status = converter->Initialize(frame, GUID_WICPixelFormat24bppBGR,
                                           WICBitmapDitherTypeNone, nullptr, 0.0,
                                           WICBitmapPaletteTypeCustom);
            source = converter;
        }
    }

    cv::Mat result;
    UINT width = 0;
    UINT height = 0;
    if (source != nullptr && SUCCEEDED(status)) status = source->GetSize(&width, &height);
    if (SUCCEEDED(status) && width > 0 && height > 0) {
        result.create(static_cast<int>(height), static_cast<int>(width), CV_8UC3);
        status = source->CopyPixels(nullptr, static_cast<UINT>(result.step),
                                    static_cast<UINT>(result.total() * result.elemSize()), result.data);
        if (FAILED(status)) result.release();
    }

    release(converter);
    release(transform);
    release(destinationColor);
    release(sourceColor);
    release(frame);
    release(decoder);
    release(factory);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return result.empty() ? cv::imread(path, cv::IMREAD_COLOR) : result;
}

}