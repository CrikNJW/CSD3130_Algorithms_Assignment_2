#include "image.h"

#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace seam_carver
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        void check(HRESULT result, const char *message)
        {
            if (FAILED(result))
            {
                std::ostringstream error;
                error << message << " (Windows error 0x" << std::hex
                      << static_cast<unsigned long>(result) << ").";
                throw std::runtime_error(error.str());
            }
        }

        struct ComSession
        {
            HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            ComSession()
            {
                if (status != RPC_E_CHANGED_MODE)
                    check(status, "Cannot initialize image codecs");
            }
            ~ComSession()
            {
                if (SUCCEEDED(status))
                    CoUninitialize();
            }
        };

        ComPtr<IWICImagingFactory> factory()
        {
            ComPtr<IWICImagingFactory> value;
            check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(value.GetAddressOf())),
                  "Cannot create image decoder");
            return value;
        }

        UINT buffer_size(std::size_t width, std::size_t height)
        {
            constexpr auto limit = std::numeric_limits<UINT>::max();
            if (width == 0 || height == 0 || width > limit / 4 || height > limit / (width * 4))
                throw std::runtime_error("Image dimensions exceed the supported buffer size.");
            return static_cast<UINT>(width * height * 4);
        }
    } // namespace

    Image load_image(const std::filesystem::path &path)
    {
        ComSession session;
        auto codec = factory();
        ComPtr<IWICBitmapDecoder> decoder;
        check(codec->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()),
              "Cannot open image; select a readable PNG, JPEG, BMP, GIF, or TIFF file");
        ComPtr<IWICBitmapFrameDecode> frame;
        check(decoder->GetFrame(0, frame.GetAddressOf()), "Cannot read first image frame");
        UINT width = 0, height = 0;
        check(frame->GetSize(&width, &height), "Cannot read image dimensions");
        const UINT bytes = buffer_size(width, height);
        ComPtr<IWICFormatConverter> converter;
        check(codec->CreateFormatConverter(converter.GetAddressOf()), "Cannot convert image");
        check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom),
              "Unsupported image pixel format");
        Image image{width, height, {}};
        image.pixels.resize(static_cast<std::size_t>(width) * height);
        static_assert(sizeof(Pixel) == 4);
        check(converter->CopyPixels(nullptr, width * 4, bytes,
                                    reinterpret_cast<BYTE *>(image.pixels.data())),
              "Cannot decode image pixels");
        return image;
    }

    void save_png(const Image &image, const std::filesystem::path &path)
    {
        validate_image(image);
        const UINT bytes = buffer_size(image.width, image.height);
        ComSession session;
        auto codec = factory();
        ComPtr<IWICStream> stream;
        check(codec->CreateStream(stream.GetAddressOf()), "Cannot create output stream");
        check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "Cannot open output file");
        ComPtr<IWICBitmapEncoder> encoder;
        check(codec->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf()),
              "Cannot create PNG encoder");
        check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Cannot initialize PNG encoder");
        ComPtr<IWICBitmapFrameEncode> frame;
        check(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr), "Cannot create PNG frame");
        check(frame->Initialize(nullptr), "Cannot initialize PNG frame");
        check(frame->SetSize(static_cast<UINT>(image.width), static_cast<UINT>(image.height)),
              "Cannot set PNG dimensions");
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        check(frame->SetPixelFormat(&format), "Cannot set PNG pixel format");
        if (!IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA))
            throw std::runtime_error("PNG encoder does not support BGRA pixels.");
        check(frame->WritePixels(static_cast<UINT>(image.height), static_cast<UINT>(image.width * 4),
                                 bytes, reinterpret_cast<BYTE *>(const_cast<Pixel *>(image.pixels.data()))),
              "Cannot write PNG pixels");
        check(frame->Commit(), "Cannot finish PNG frame");
        check(encoder->Commit(), "Cannot save PNG file");
    }

} // namespace seam_carver
