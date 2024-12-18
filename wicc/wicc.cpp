#include "targetver.h"
#include <iostream>
#include <iomanip>
#include <string_view>
#include <wincodec.h>
#include <atlbase.h>
#include <bit>
#include <system_error>

class COMContext
{
public:
	COMContext()
	{
		auto hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	}
	~COMContext() noexcept
	{
		CoUninitialize();
	}
};

std::wstring guidTostring(const GUID& guid)
{
	WCHAR str[40]{};
	if(StringFromGUID2(guid, str, static_cast<int>(std::size(str))) == 0)
		std::wcout << L"failed: StringFromGUID2\n";
	return str;
}

std::string hresultTostring(HRESULT hr)
{
	return std::system_category().message(hr);
}

bool getCodecinfo(CComPtr<IWICBitmapCodecInfo> info)
{
	UINT length;
	WCHAR buffer[1024];
	HRESULT hr = info->GetAuthor(static_cast<UINT>(std::size(buffer)), buffer, &length);
	if(hr != S_OK)
	{
		std::wcout << L"failed: GetAuthor\n";
		return false;
	}
	std::wcout << L"Author: " << std::wstring_view(buffer, length) << std::endl;

	hr = info->GetFriendlyName(static_cast<UINT>(std::size(buffer)), buffer, &length);
	if(hr != S_OK)
	{
		std::wcout << L"failed: GetFriendlyName\n";
		return false;
	}
	std::wcout << L"Name: " << std::wstring_view(buffer, length) << std::endl;

	hr = info->GetFileExtensions(static_cast<UINT>(std::size(buffer)), buffer, &length);
	if(hr != S_OK)
	{
		std::wcout << L"failed: GetFileExtensions\n";
		return false;
	}
	std::wcout << L"Extensions: " << std::wstring_view(buffer, length) << std::endl;

	hr = info->GetVersion(static_cast<UINT>(std::size(buffer)), buffer, &length);
	if(hr != S_OK)
	{
		std::wcout << L"failed: GetVersion\n";
		return false;
	}
	std::wcout << L"Version: " << std::wstring_view(buffer, length) << std::endl;

	CLSID clsid;
	hr = info->GetCLSID(&clsid);
	if(hr != S_OK)
	{
		std::wcout << L"failed: GetCLSID\n";
		return false;
	}

	auto decoderguid = guidTostring(clsid);
	std::wcout << L"CLSID: " << decoderguid << std::endl;

	return true;
}

void enumComponets(IWICImagingFactory* factory)
{
	CComPtr<IEnumUnknown> enumPtr;
	HRESULT hr = factory->CreateComponentEnumerator(WICDecoder | WICEncoder, WICComponentEnumerateDefault, &enumPtr);
	if(FAILED(hr))
	{
		std::wcout << L"failed: CreateComponentEnumerator\n";
		return;
	}

	ULONG uLong;
	CComPtr<IUnknown> component;
	while(enumPtr->Next(1, &component, &uLong) == S_OK)
	{
		CComQIPtr<IWICBitmapCodecInfo> info(component);
		getCodecinfo(info);
		std::wcout << std::endl;
	}
}

bool decoderinfo(CComPtr<IWICBitmapDecoder> decoder)
{
	CComPtr<IWICBitmapDecoderInfo> dinfo;
	HRESULT hr = decoder->GetDecoderInfo(&dinfo);
	if(FAILED(hr))
	{
		std::wcout << L"failed: IWICBitmapDecoder::GetDecoderInfo\n";
		return false;
	}

	CComQIPtr<IWICBitmapCodecInfo> info(dinfo);
	return getCodecinfo(info);
}

uint16_t readBE16(const uint8_t* ptr, size_t& pos)
{
	auto result = std::byteswap(*(uint16_t*)(ptr + pos));
	pos += 2;
	return result;
}
uint32_t readBE32(const uint8_t* ptr, size_t& pos)
{
	auto result = std::byteswap(*(uint32_t*)(ptr + pos));
	pos += 4;
	return result;
}

bool parseICCProfile(const uint8_t* ptr, size_t length)
{
	// https://www.color.org/ICC1V42.pdf
	if(length < 128 + 4)
		return false;

	size_t pos = 0;
	auto headerLength = readBE32(ptr, pos);
	if(headerLength != length)
		return false;

	pos = 128;
	auto tagCount = readBE32(ptr, pos);
	for(size_t i = 0; i < tagCount; i++)
	{
		auto sig = readBE32(ptr, pos);
		auto offset = readBE32(ptr, pos);
		auto length = readBE32(ptr, pos);
		if(sig == 0x64657363) //'desc'
		{
			pos = offset;
			auto sig = readBE32(ptr, pos);
			if(sig == 0x64657363) //'desc' 7bit text
			{
				pos += 4; // 0
				auto descLength = readBE32(ptr, pos);
				std::cout << "color profile: " << reinterpret_cast<const char*>(ptr + pos) << std::endl;
				return true;
			} else if(sig == 0x6D6C7563)
			{
				//'mluc' multiLocalizedUnicodeType
				// support first record only
				pos += 4; // 0
				pos += 4; // number of names
				pos += 4; // 0xC name record size
				pos += 2; //言語コード：ISO 639-1
				pos += 2; //国名コード：ISO-3166/JIS X 0304
				auto descLength = readBE32(ptr, pos);
				auto descOffset = readBE32(ptr, pos);
				pos = offset + descOffset;
				std::wstring description;
				for(size_t j = 0; j < descLength; j += 2)
					description.push_back(readBE16(ptr, pos));
				std::wcout << L"color profile: " << description << std::endl;
			}
			break;
		}
	}
	return false;
}

bool load(IWICImagingFactory* factory, CComPtr<IWICBitmapDecoder> decoder)
{
	UINT FrameCount = 0;
	HRESULT hr = decoder->GetFrameCount(&FrameCount);
	if(FAILED(hr))
	{
		std::wcout << L"failed: IWICBitmapDecoder::GetFrameCount\n";
		return false;
	}

	CComPtr<IWICBitmapFrameDecode> frame;

	for(UINT i = 0; i < FrameCount; i++)
	{
		hr = decoder->GetFrame(i, &frame);
		if(FAILED(hr))
		{
			std::wcout << L"failed: IWICBitmapDecoder::GetFrame\n";
			return false;
		}

		UINT width, height;
		hr = frame->GetSize(&width, &height);
		if(FAILED(hr))
		{
			std::wcout << L"failed: IWICBitmapSource::GetSize\n";
			return false;
		}

		std::wcout << L"Frame: " << i << L" Width: " << width << L" Height: " << height;
		std::wcout << std::endl;

		CComPtr<IWICBitmap> bitmap;
		hr = factory->CreateBitmapFromSource(frame, WICBitmapCacheOnDemand, &bitmap);
		if(FAILED(hr))
		{
			std::wcout << L"failed: CreateBitmapFromSource\n";
			return false;
		}

		CComPtr<IWICBitmapLock> lock;
		WICRect rcLock{ 0, 0, (INT)width, (INT)height };
		hr = bitmap->Lock(&rcLock, WICBitmapLockRead, &lock);
		if(FAILED(hr))
		{
			std::wcout << L"failed: IWICBitmap::Lock\n";
			return false;
		}

		UINT buffersize = 0;
		UINT stride;
		uint8_t* pv = nullptr;
		hr = lock->GetStride(&stride);
		if(FAILED(hr))
		{
			std::wcout << L"failed: IWICBitmapLock::GetStride\n";
			return false;
		}

		hr = lock->GetDataPointer(&buffersize, &pv);
		if(FAILED(hr))
		{
			std::wcout << L"failed: IWICBitmapLock::GetDataPointer\n";
			return false;
		}
		CComPtr<IWICColorContext> colorContext;
		hr = factory->CreateColorContext(&colorContext);
		if(FAILED(hr))
		{
			std::wcout << L"failed: CreateColorContext\n";
			return false;
		}
		UINT ccCount = 0;
		// need IWICColorContext ptr
		hr = frame->GetColorContexts(1, &(colorContext.p), &ccCount);
		if(SUCCEEDED(hr) && ccCount == 1)
		{
			UINT profileLength = 0;
			hr = colorContext->GetProfileBytes(0, nullptr, &profileLength);
			if(SUCCEEDED(hr)) // heicで何故か失敗するのでエラー扱いにしない
			{
				auto profile = std::make_unique<uint8_t[]>(profileLength);
				hr = colorContext->GetProfileBytes(profileLength, profile.get(), &profileLength);
				if(SUCCEEDED(hr))
					parseICCProfile(profile.get(), profileLength);
			}
		}
	}
	return true;
}

int wmain(int argc, wchar_t* argv[])
{
	std::wcout.imbue(std::locale(""));
	if(argc <= 1)
	{
		std::wcout << L"Syntax:  wicc [options or files ...]" << std::endl
			<< std::endl;
		std::wcout << L"Options: begins with [- or /]" << std::endl;
		std::wcout << L" l, L    List installed Components" << std::endl;
		std::wcout << L" i, I    Show Componentinfo" << std::endl;
		std::wcout << L" m, M    Preferred Microsoft Component" << std::endl;
		std::wcout << L" oldpng  Use Windows7 PNG Decoder" << std::endl;
		return 0;
	}

	COMContext comCtx;
	CComPtr<IWICImagingFactory> factory;
	HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if(FAILED(hr))
	{
		std::wcout << L"failed: CoCreateInstance(IWICImagingFactory)\n";
		std::cout << "HRESULT:" << hresultTostring(hr) << std::endl;
		return hr;
	}

	LARGE_INTEGER freq, before, after;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&before);

	bool force_oldpng = false;
	bool show_componentinfo = false;
	const GUID* preferred_vender = nullptr;
	for(int i = 1; i < argc; i++)
	{
		auto arg = argv[i];
		if(arg[0] == L'-' || arg[0] == L'/')
		{
			if(_wcsicmp(arg + 1, L"oldpng") == 0)
				force_oldpng = true;
			else if(arg[1] == L'l' || arg[1] == L'L')
				enumComponets(factory);
			else if(arg[1] == L'i' || arg[1] == L'I')
				show_componentinfo = true;
			else if(arg[1] == L'm' || arg[1] == L'M')
				preferred_vender = &GUID_VendorMicrosoft;
		} else
		{
			CComPtr<IWICBitmapDecoder> decoder;
			CComPtr<IWICStream> stream;
			std::wcout << arg << std::endl;
			if(force_oldpng)
			{
				hr = factory->CreateDecoder(CLSID_WICPngDecoder1, nullptr, &decoder);
				hr = decoder.CoCreateInstance(CLSID_WICPngDecoder1);
				if(FAILED(hr))
				{
					std::wcout << L"failed: CreateDecoder(CLSID_WICPngDecoder1)\n";
					break;
				}

				hr = factory->CreateStream(&stream);
				if(FAILED(hr))
				{
					std::wcout << L"failed: CreateStream\n";
					break;
				}
				hr = stream->InitializeFromFilename(arg, GENERIC_READ);
				if(FAILED(hr))
				{
					std::wcout << L"failed: IWICStream::InitializeFromFilename\n";
					break;
				}
				decoder->Initialize(stream, WICDecodeMetadataCacheOnDemand);
				if(FAILED(hr))
				{
					std::wcout << L"failed: IWICBitmapDecoder::Initialize\n";
					break;
				}
			} else
			{
				hr = factory->CreateDecoderFromFilename(arg, preferred_vender, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
				if(FAILED(hr))
				{
					std::wcout << L"failed: CreateDecoderFromFilename\n";
					std::cout << "HRESULT:" << hresultTostring(hr) << std::endl;
				}
			}
			if(SUCCEEDED(hr))
			{
				if(show_componentinfo)
					decoderinfo(decoder);
				load(factory, decoder);
			}
			std::wcout << std::endl;
		}
	}

	QueryPerformanceCounter(&after);
	if(freq.QuadPart != 0)
	{
		double time = (double)(after.QuadPart - before.QuadPart) / (double)freq.QuadPart;
		std::wcout << L"TotalTime: " << std::fixed << time;
	}

	return 0;
}
