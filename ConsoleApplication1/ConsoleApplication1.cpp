#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <iostream>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")

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

void GenerateTextImage(const char* text, const char* fontName, int fontSize, const char* outputFileName) {
    std::wstring wText = StringToWString(text);
    std::wstring wFontName = StringToWString(fontName);
    std::wstring wOutputFileName = StringToWString(outputFileName);

    // 初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    {
        Gdiplus::FontFamily fontFamily(wFontName.c_str());
        Gdiplus::Font font(&fontFamily, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        // 取得文字尺寸(pixel)
        Gdiplus::SizeF textSize = MeasureString(wText, font);

        // 留白邊
        int width = static_cast<int>(textSize.Width + 1);
        int height = static_cast<int>(textSize.Height + 1);

        Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(&bitmap);

        // 设置背景颜色
        graphics.Clear(Gdiplus::Color(255, 255, 255, 255)); // 白色背景

        Gdiplus::StringFormat stringFormat;
        stringFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        stringFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        // 文字顏色設定
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 0, 0)); // 黑色

        // 繪圖
        Gdiplus::RectF rect(0, 0, static_cast<Gdiplus::REAL>(width), static_cast<Gdiplus::REAL>(height));
        graphics.DrawString(wText.c_str(), -1, &font, rect, &stringFormat, &brush);
        // 轉換為單色Bmp
        Gdiplus::Bitmap* monoBitmap = nullptr;
        ConvertToMonoBitmap(&bitmap, monoBitmap);

        // 文件存檔 , 後續要改
        CLSID clsid;
        CLSIDFromString(L"{557CF400-1A04-11D3-9A73-0000F81EF32E}", &clsid); 
        monoBitmap->Save(wOutputFileName.c_str(), &clsid, NULL);
    }
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

int main() {
    const char* text = "Vegetable Super Man";
    const char* fontName = "Arial";
    int fontSize = 40;
    const char* outputFileName = "output.bmp";

    GenerateTextImage(text, fontName, fontSize, outputFileName);

    std::cout << "Text image generated and saved to " << outputFileName << std::endl;

    return 0;
}
