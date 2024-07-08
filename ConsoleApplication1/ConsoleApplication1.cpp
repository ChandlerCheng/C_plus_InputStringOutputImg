#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#pragma comment(lib, "gdiplus.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::wstring StringToWString(const std::string& str) {
    int len;
    int slength = (int)str.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), slength, 0, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), slength, &wstr[0], len);
    return wstr;
}

Gdiplus::SizeF MeasureString(const std::wstring& text, const Gdiplus::Font& font) {
    Gdiplus::Bitmap dummyBitmap(1, 1);
    Gdiplus::Graphics dummyGraphics(&dummyBitmap);
    Gdiplus::RectF layoutRect(0, 0, 1000, 1000); // 臨時尺寸
    Gdiplus::RectF boundingBox;
    dummyGraphics.MeasureString(text.c_str(), -1, &font, layoutRect, &boundingBox);
    return Gdiplus::SizeF(boundingBox.Width, boundingBox.Height);
}

void ConvertToMonoBitmap(Gdiplus::Bitmap* sourceBitmap, Gdiplus::Bitmap*& monoBitmap) {
    UINT width = sourceBitmap->GetWidth();
    UINT height = sourceBitmap->GetHeight();

    monoBitmap = new Gdiplus::Bitmap(width, height, PixelFormat1bppIndexed);

    Gdiplus::BitmapData sourceData;
    Gdiplus::Rect rect(0, 0, width, height);
    sourceBitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &sourceData);

    Gdiplus::BitmapData monoData;
    monoBitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat1bppIndexed, &monoData);

    BYTE* sourceScan0 = (BYTE*)sourceData.Scan0;
    BYTE* monoScan0 = (BYTE*)monoData.Scan0;

    for (UINT y = 0; y < height; ++y) {
        BYTE* sourceRow = sourceScan0 + y * sourceData.Stride;
        BYTE* monoRow = monoScan0 + y * monoData.Stride;

        for (UINT x = 0; x < width; ++x) {
            BYTE blue = sourceRow[x * 3];
            BYTE green = sourceRow[x * 3 + 1];
            BYTE red = sourceRow[x * 3 + 2];

            BYTE gray = (BYTE)(0.299 * red + 0.587 * green + 0.114 * blue);

            if (gray > 128) {
                monoRow[x / 8] |= (128 >> (x % 8));
            }
        }
    }

    sourceBitmap->UnlockBits(&sourceData);
    monoBitmap->UnlockBits(&monoData);
}

Gdiplus::Bitmap* GenerateTextImage(const char* text, const char* fontName, int fontSize, int rotationAngle) {
    std::wstring wText = StringToWString(text);
    std::wstring wFontName = StringToWString(fontName);

    Gdiplus::FontFamily fontFamily(wFontName.c_str());
    Gdiplus::Font font(&fontFamily, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    // 取得文字尺寸(pixel)
    Gdiplus::SizeF textSize = MeasureString(wText, font);

    // 計算旋轉後的尺寸
    double radians = rotationAngle * M_PI / 180.0;
    int width = static_cast<int>(std::abs(textSize.Width * std::cos(radians)) + std::abs(textSize.Height * std::sin(radians))) + 1;
    int height = static_cast<int>(std::abs(textSize.Width * std::sin(radians)) + std::abs(textSize.Height * std::cos(radians))) + 1;

    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(bitmap);

    // 背景顏色設定
    graphics.Clear(Gdiplus::Color(255, 255, 255, 255)); // 白色背景

    Gdiplus::StringFormat stringFormat;
    stringFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
    stringFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    // 文字顏色設定
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 0, 0)); // 黑色

    // Apply rotation
    Gdiplus::GraphicsState state = graphics.Save();
    graphics.TranslateTransform(width / 2, height / 2);
    graphics.RotateTransform(static_cast<Gdiplus::REAL>(rotationAngle));
    graphics.TranslateTransform(-textSize.Width / 2, -textSize.Height / 2);

    // 繪製圖片
    Gdiplus::RectF rect(0, 0, static_cast<Gdiplus::REAL>(textSize.Width), static_cast<Gdiplus::REAL>(textSize.Height));
    graphics.DrawString(wText.c_str(), -1, &font, rect, &stringFormat, &brush);

    graphics.Restore(state);

    // 轉換成單色
    Gdiplus::Bitmap* monoBitmap = nullptr;
    ConvertToMonoBitmap(bitmap, monoBitmap);

    delete bitmap;
    return monoBitmap;
}

// Helper function to get the CLSID of the encoder
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0; // number of image encoders
    UINT size = 0; // size of the image encoder array in bytes

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1; // Failure

    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1; // Failure

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j; // Success
        }
    }

    free(pImageCodecInfo);
    return -1; // Failure
}

int GetWindowsFontData(const char* text, const char* fontName, int fontSize, unsigned char** dstAddress, int* widthOut, int* heightOut, int rotationAngle) {
    unsigned char* bitmapData = nullptr;
    int result = 0;
    // 初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 產生文字的Bitmap圖片
    Gdiplus::Bitmap* monoBitmap = GenerateTextImage(text, fontName, fontSize, rotationAngle);
#if 0
    // 保存圖片到檔案
    CLSID bmpClsid;
    GetEncoderClsid(L"image/bmp", &bmpClsid);
    monoBitmap->Save(L"D:\\Project\\GiLab\\C_plus_InputStringOutputImg\\ConsoleApplication1\\output.bmp", &bmpClsid, NULL);
#endif
    // 計算寬高 
    UINT width = monoBitmap->GetWidth();
    UINT height = monoBitmap->GetHeight();
    UINT nPadded = ((width + 7) / 8);
    UINT stride = ((nPadded + 3) / 4) * 4;

    bitmapData = (unsigned char*)malloc(height * stride);
    *widthOut = stride;
    *heightOut = height;

    if (bitmapData) {
        Gdiplus::Rect rect(0, 0, width, height);
        Gdiplus::BitmapData monoData;
        monoBitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat1bppIndexed, &monoData);

        BYTE* monoScan0 = (BYTE*)monoData.Scan0;

        for (UINT y = 0; y < height; ++y) {
            BYTE* monoRow = monoScan0 + y * monoData.Stride;
            memcpy(bitmapData + y * stride, monoRow, stride);
        }

        monoBitmap->UnlockBits(&monoData);
        result = height * stride;
        *dstAddress = bitmapData;
    }

    delete monoBitmap;

    Gdiplus::GdiplusShutdown(gdiplusToken);

    return result;
}

int main() {
    const char* text = "DDDDDDDDDDDDDDD C# TEXT ";
    const char* fontName = "Arial";
    unsigned char* bitmapData = nullptr;
    int size = 0;
    int fontSize = 40, width, height;
    int rotationAngle = 180; // Set rotation angle here
    size = GetWindowsFontData(text, fontName, fontSize, &bitmapData, &width, &height, rotationAngle);
    free(bitmapData);
    return 0;
}
