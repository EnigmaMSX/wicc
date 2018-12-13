#include "targetver.h"
#include <iostream>
#include <iomanip>
#include <string_view>
#include <wincodec.h>
#include <atlbase.h>

std::wstring guidtostring(const GUID& guid)
{
	WCHAR str[40] {};
	if(StringFromGUID2(guid, str, static_cast<int>(std::size(str))) == 0)
		std::wcout << L"failed: StringFromGUID2\n";
	return str;
}

bool codecinfo(CComPtr<IWICBitmapCodecInfo> info)
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

	auto decoderguid = guidtostring(clsid);
	std::wcout << L"CLSID: " << decoderguid.c_str() << std::endl;

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
		codecinfo(info);
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
	return codecinfo(info);
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
		uint8_t *pv = nullptr;
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

	}
	return true;
}

int wmain(int argc, wchar_t* argv[])
{
	std::wcout.imbue(std::locale(""));
	if(argc <= 1)
	{
		std::wcout << L"Syntax:  wicc [options or files ...]" << std::endl << std::endl;
		std::wcout << L"Options: begins with [- or /]" << std::endl;
		std::wcout << L" l, L    List installed Components" << std::endl;
		std::wcout << L" c, C    Show Componentinfo" << std::endl;
		std::wcout << L" m, M    Preferred Microsoft Component" << std::endl;
		std::wcout << L" oldpng  Use Windows7 PNG Decoder" << std::endl;
		return 0;
	}

	HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if(FAILED(hr))
	{
		std::wcout << L"failed: CoInitializeEx\n";
		goto exit;
	}

	IWICImagingFactory* factory = nullptr;
	hr = ::CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if(FAILED(hr))
	{
		std::wcout << L"failed: CoCreateInstance(IWICImagingFactory)\n";
		goto exit;
	}

	LARGE_INTEGER freq, before, after;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&before);

	bool force_oldpng = false;
	bool show_componentinfo = false;
	const GUID* preferred_vender = nullptr;
	for(size_t i = 1; i < argc; i++)
	{
		auto arg = argv[i];
		if(arg[0] == L'-'|| arg[0] == L'/')
		{
			if(_wcsicmp(arg + 1, L"oldpng") == 0)
				force_oldpng = true;
			else if(arg[1] == L'l' || arg[1] == L'L')
				enumComponets(factory);
			else if(arg[1] == L'c' || arg[1] == L'C')
				show_componentinfo = true;
			else if(arg[1] == L'm' || arg[1] == L'M')
				preferred_vender = &GUID_VendorMicrosoft;
		}else
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
					goto exit;
				}

				hr = factory->CreateStream(&stream);
				if(FAILED(hr))
				{
					std::wcout << L"failed: CreateStream\n";
					goto exit;
				}
				hr = stream->InitializeFromFilename(arg, GENERIC_READ);
				if(FAILED(hr))
				{
					std::wcout << L"failed: IWICStream::InitializeFromFilename\n";
					goto exit;
				}
				decoder->Initialize(stream, WICDecodeMetadataCacheOnDemand);
				if(FAILED(hr))
				{
					std::wcout << L"failed: IWICBitmapDecoder::Initialize\n";
					goto exit;
				}
			}else
			{
				hr = factory->CreateDecoderFromFilename(arg, preferred_vender, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
				if(FAILED(hr))
				{
					std::wcout << L"failed: CreateDecoderFromFilename\n";
					goto exit;
				}
			}
			if(show_componentinfo)
				decoderinfo(decoder);
			load(factory, decoder);
			std::wcout << std::endl;
		}
	}
exit:
	QueryPerformanceCounter(&after);
	if(freq.QuadPart != 0)
	{
		double time = (double)(after.QuadPart - before.QuadPart) / (double)freq.QuadPart;
		std::wcout << L"TotalTime: " << std::fixed << time;
	}
	if(factory)
		factory->Release();
	CoUninitialize();
	return 0;
}

