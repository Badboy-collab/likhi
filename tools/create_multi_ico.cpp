#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

struct IconDirEntry {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
};

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (!pImageCodecInfo) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

int main() {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Bitmap* src = Bitmap::FromFile(L"assets/icon_source.jpg");
    if (!src || src->GetLastStatus() != Ok) {
        std::cerr << "Failed to load assets/icon_source.jpg\n";
        return 1;
    }

    CLSID pngClsid, bmpClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    GetEncoderClsid(L"image/bmp", &bmpClsid);

    // Save master icon.png (512x512)
    {
        Bitmap masterPng(512, 512, PixelFormat32bppARGB);
        Graphics g(&masterPng);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(SmoothingModeHighQuality);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(src, 0, 0, 512, 512);
        masterPng.Save(L"assets/icon.png", &pngClsid, NULL);
        std::cout << "Saved assets/icon.png\n";
    }

    // Save logo_sidebar.bmp (48x48)
    {
        Bitmap sbBmp(48, 48, PixelFormat24bppRGB);
        Graphics g(&sbBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(SmoothingModeHighQuality);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(src, 0, 0, 48, 48);
        sbBmp.Save(L"assets/logo_sidebar.bmp", &bmpClsid, NULL);
        std::cout << "Saved assets/logo_sidebar.bmp\n";
    }

    // Save logo_about.bmp (96x96)
    {
        Bitmap abBmp(96, 96, PixelFormat24bppRGB);
        Graphics g(&abBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(SmoothingModeHighQuality);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(src, 0, 0, 96, 96);
        abBmp.Save(L"assets/logo_about.bmp", &bmpClsid, NULL);
        std::cout << "Saved assets/logo_about.bmp\n";
    }

    // Generate multi-resolution icon.ico
    std::vector<int> sizes = {16, 20, 24, 32, 48, 64, 128, 256};
    std::vector<std::vector<BYTE>> image_data;

    for (int sz : sizes) {
        Bitmap target(sz, sz, PixelFormat32bppARGB);
        Graphics g(&target);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(SmoothingModeHighQuality);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(src, 0, 0, sz, sz);

        IStream* pStream = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        target.Save(pStream, &pngClsid, NULL);

        STATSTG stat;
        pStream->Stat(&stat, STATFLAG_NONAME);
        ULONG bytes = (ULONG)stat.cbSize.QuadPart;

        std::vector<BYTE> buf(bytes);
        LARGE_INTEGER liZero = {0};
        pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
        ULONG read = 0;
        pStream->Read(buf.data(), bytes, &read);
        pStream->Release();

        image_data.push_back(buf);
        std::cout << "Generated icon size " << sz << "x" << sz << " (" << bytes << " bytes)\n";
    }

    delete src;

    // Write .ICO File
    std::ofstream out("assets/icon.ico", std::ios::binary);
    WORD idReserved = 0;
    WORD idType = 1;
    WORD idCount = (WORD)sizes.size();

    out.write((char*)&idReserved, 2);
    out.write((char*)&idType, 2);
    out.write((char*)&idCount, 2);

    DWORD offset = 6 + (DWORD)(sizes.size() * sizeof(IconDirEntry));

    for (size_t i = 0; i < sizes.size(); i++) {
        IconDirEntry entry;
        int sz = sizes[i];
        entry.bWidth = (sz >= 256) ? 0 : (BYTE)sz;
        entry.bHeight = (sz >= 256) ? 0 : (BYTE)sz;
        entry.bColorCount = 0;
        entry.bReserved = 0;
        entry.wPlanes = 1;
        entry.wBitCount = 32;
        entry.dwBytesInRes = (DWORD)image_data[i].size();
        entry.dwImageOffset = offset;

        out.write((char*)&entry, sizeof(entry));
        offset += entry.dwBytesInRes;
    }

    for (size_t i = 0; i < image_data.size(); i++) {
        out.write((char*)image_data[i].data(), image_data[i].size());
    }

    out.close();
    std::cout << "Successfully saved new multi-resolution assets/icon.ico!\n";

    GdiplusShutdown(gdiplusToken);
    return 0;
}
