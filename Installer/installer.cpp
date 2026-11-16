#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <system_error>
#include "payload.h"

namespace fs = std::filesystem;
namespace {
constexpr int artWidth = 1737, artHeight = 664, cornerCut = 18;
HINSTANCE instance;
Gdiplus::Bitmap* artwork = nullptr;
IStream* artworkStream = nullptr;
int pressed = 0;
bool installing = false;
bool layeredWindow = false;

std::wstring errorText(const std::exception& e) {
    int n = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
    if (n <= 0) return L"Unknown error";
    std::wstring result(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, result.data(), n);
    result.pop_back();
    return result;
}

void winCheck(bool ok, const char* message) {
    if (!ok) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), message);
}

fs::path uniquePath(const fs::path& directory) {
    wchar_t name[MAX_PATH];
    winCheck(GetTempFileNameW(directory.c_str(), L"DAA", 0, name) != 0, "Cannot create temporary path");
    winCheck(DeleteFileW(name) != 0, "Cannot prepare temporary path");
    return fs::path(name);
}

void extractFile(const PayloadFile& file, const fs::path& root) {
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(file.resourceId), RT_RCDATA);
    if (!resource) throw std::runtime_error("An embedded plugin file is missing");
    HGLOBAL loaded = LoadResource(instance, resource);
    DWORD size = SizeofResource(instance, resource);
    const void* data = LockResource(loaded);
    if (!data || size == 0) throw std::runtime_error("An embedded plugin file is empty or invalid");
    fs::path output = root / file.relativePath;
    fs::create_directories(output.parent_path());
    HANDLE handle = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    winCheck(handle != INVALID_HANDLE_VALUE, "Cannot create plugin file");
    DWORD written = 0;
    BOOL ok = WriteFile(handle, data, size, &written, nullptr);
    DWORD error = GetLastError();
    CloseHandle(handle);
    if (!ok || written != size) {
        if (ok) error = ERROR_WRITE_FAULT;
        throw std::system_error(static_cast<int>(error), std::system_category(), "Cannot write plugin file");
    }
}

fs::path standardPluginDirectory() {
    PWSTR common = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, nullptr, &common);
    if (FAILED(hr)) throw std::runtime_error("Cannot find the common Program Files folder");
    fs::path directory = fs::path(common) / L"VST3";
    CoTaskMemFree(common);
    return directory;
}

fs::path installPlugin(const fs::path& directory) {
    fs::create_directories(directory);
    const fs::path target = directory / L"D.A AURA.vst3";
    const fs::path stage = uniquePath(directory);
    fs::path backup;
    bool oldMoved = false;
    try {
        fs::create_directory(stage);
        for (const auto& file : payloadFiles) extractFile(file, stage);
        if (fs::exists(target)) {
            backup = uniquePath(directory);
            fs::rename(target, backup);
            oldMoved = true;
        }
        fs::rename(stage, target);
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(stage, ignored);
        if (oldMoved) {
            std::error_code restoreError;
            fs::rename(backup, target, restoreError);
            if (restoreError) throw std::runtime_error("Installation failed and the previous version could not be restored. The previous version is preserved in a DAA temporary folder in the VST3 directory.");
        }
        throw;
    }
    if (oldMoved) {
        std::error_code ignored;
        fs::remove_all(backup, ignored);
    }
    return target;
}

// Hit areas are expressed in coordinates of the 1737x664 installer artwork.
int hitTest(HWND hwnd, int x, int y) {
    RECT r; GetClientRect(hwnd, &r);
    if (r.right <= 0 || r.bottom <= 0) return 0;
    x = MulDiv(x, artWidth, r.right);
    y = MulDiv(y, artHeight, r.bottom);
    if (x >= 848 && x <= 1243 && y >= 536 && y <= 607) return 1; // Next ->
    if (x >= 1280 && x <= 1645 && y >= 536 && y <= 607) return 2; // Cancel
    if (x >= 1656 && x <= 1710 && y >= 14 && y <= 62) return 3;   // Close (X)
    if (x >= 1592 && x <= 1650 && y >= 14 && y <= 62) return 4;   // Minimize (-)
    return 0;
}

void activate(HWND hwnd, int action) {
    if (installing) return;
    if (action == 2 || action == 3) { DestroyWindow(hwnd); return; }
    if (action == 4) { ShowWindow(hwnd, SW_MINIMIZE); return; }
    if (action != 1) return;
    installing = true;
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    try {
        fs::path target = installPlugin(standardPluginDirectory());
        std::wstring message = L"D.A AURA has been installed:\n\n" + target.wstring() + L"\n\nRestart or rescan your DAW to use D.A AURA.";
        MessageBoxW(hwnd, message.c_str(), L"D.A AURA Installer", MB_OK | MB_ICONINFORMATION);
        DestroyWindow(hwnd);
    } catch (const std::exception& e) {
        std::wstring message = L"Installation failed. No new plugin was installed.\n\n" + errorText(e);
        MessageBoxW(hwnd, message.c_str(), L"D.A AURA Installer", MB_OK | MB_ICONERROR);
        installing = false;
    }
}

// Explicit pixel units and a source rectangle prevent GDI+ from applying the
// bitmap/screen DPI ratio a second time on high-DPI displays.
void drawArtwork(Gdiplus::Graphics& graphics, int width, int height) {
    graphics.SetPageUnit(Gdiplus::UnitPixel);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.DrawImage(artwork, Gdiplus::Rect(0, 0, width, height),
                       0, 0, artWidth, artHeight, Gdiplus::UnitPixel);
}

// Render the octagonal RGBA artwork with per-pixel alpha so the 45-degree
// chamfered corners blend cleanly onto the desktop with no outer window frame.
bool applyLayeredWindow(HWND hwnd, int posX, int posY, int width, int height) {
    Gdiplus::Bitmap surface(width, height, PixelFormat32bppPARGB);
    if (surface.GetLastStatus() != Gdiplus::Ok) return false;
    {
        Gdiplus::Graphics graphics(&surface);
        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
        drawArtwork(graphics, width, height);
    }
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* dibBits = nullptr;
    HBITMAP dib = memDc ? CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0) : nullptr;
    bool ok = false;
    if (memDc && dib && dibBits) {
        Gdiplus::Rect lockRect(0, 0, width, height);
        Gdiplus::BitmapData data = {};
        if (surface.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &data) == Gdiplus::Ok) {
            const BYTE* srcRow = static_cast<const BYTE*>(data.Scan0);
            BYTE* dstRow = static_cast<BYTE*>(dibBits);
            const size_t rowBytes = static_cast<size_t>(width) * 4;
            for (int row = 0; row < height; ++row) {
                std::memcpy(dstRow + static_cast<size_t>(row) * rowBytes,
                            srcRow + static_cast<ptrdiff_t>(row) * data.Stride,
                            rowBytes);
            }
            surface.UnlockBits(&data);
            HGDIOBJ oldBmp = SelectObject(memDc, dib);
            POINT ptPos = { posX, posY };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            ok = UpdateLayeredWindow(hwnd, screenDc, &ptPos, &sizeWin, memDc, &ptSrc, 0, &blend, ULW_ALPHA) != FALSE;
            SelectObject(memDc, oldBmp);
        }
    }
    if (dib) DeleteObject(dib);
    if (memDc) DeleteDC(memDc);
    if (screenDc) ReleaseDC(nullptr, screenDc);
    return ok;
}

bool testRendering() {
    if (static_cast<int>(artwork->GetWidth()) != artWidth ||
        static_cast<int>(artwork->GetHeight()) != artHeight) return false;
    Gdiplus::Color corner, center, nextBtn;
    if (artwork->GetPixel(0, 0, &corner) != Gdiplus::Ok || corner.GetAlpha() != 0) return false;
    if (artwork->GetPixel(artWidth - 1, 0, &corner) != Gdiplus::Ok || corner.GetAlpha() != 0) return false;
    if (artwork->GetPixel(0, artHeight - 1, &corner) != Gdiplus::Ok || corner.GetAlpha() != 0) return false;
    if (artwork->GetPixel(artWidth - 1, artHeight - 1, &corner) != Gdiplus::Ok || corner.GetAlpha() != 0) return false;
    if (artwork->GetPixel(artWidth / 2, artHeight / 2, &center) != Gdiplus::Ok || center.GetAlpha() != 255) return false;
    if (artwork->GetPixel(1045, 571, &nextBtn) != Gdiplus::Ok || nextBtn.GetRed() < 180) return false;

    const auto diff = [](BYTE a, BYTE b) { return std::abs(int(a) - int(b)); };
    for (int width : {artWidth, artWidth / 2}) {
        const int height = MulDiv(width, artHeight, artWidth);
        Gdiplus::Bitmap baseline(width, height, PixelFormat32bppARGB);
        baseline.SetResolution(96.0f, 96.0f);
        { Gdiplus::Graphics graphics(&baseline); drawArtwork(graphics, width, height); }
        if (width == artWidth) {
            for (int y : {40, 250, 420, 571, 630}) {
                for (int x : {200, 500, 1045, 1462, 1650}) {
                    Gdiplus::Color expected, actual;
                    if (artwork->GetPixel(x, y, &expected) != Gdiplus::Ok ||
                        baseline.GetPixel(x, y, &actual) != Gdiplus::Ok) return false;
                    if (diff(expected.GetRed(), actual.GetRed()) > 25 ||
                        diff(expected.GetGreen(), actual.GetGreen()) > 25 ||
                        diff(expected.GetBlue(), actual.GetBlue()) > 25) return false;
                }
            }
        }
        for (float dpi : {120.0f, 144.0f, 192.0f}) {
            Gdiplus::Bitmap target(width, height, PixelFormat32bppARGB);
            if (target.SetResolution(dpi, dpi) != Gdiplus::Ok) return false;
            { Gdiplus::Graphics graphics(&target); drawArtwork(graphics, width, height); }
            for (int y : {40, 250, 420, 571, 630}) {
                for (int x : {200, 500, 1045, 1462, 1650}) {
                    const int dx = MulDiv(x, width, artWidth);
                    const int dy = MulDiv(y, height, artHeight);
                    Gdiplus::Color expected, actual;
                    if (baseline.GetPixel(dx, dy, &expected) != Gdiplus::Ok ||
                        target.GetPixel(dx, dy, &actual) != Gdiplus::Ok) return false;
                    if (diff(expected.GetRed(), actual.GetRed()) > 2 ||
                        diff(expected.GetGreen(), actual.GetGreen()) > 2 ||
                        diff(expected.GetBlue(), actual.GetBlue()) > 2) return false;
                }
            }
        }
    }
    return true;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        if (!layeredWindow) {
            RECT r; GetClientRect(hwnd, &r);
            Gdiplus::Graphics graphics(dc);
            drawArtwork(graphics, r.right, r.bottom);
        }
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_PRINTCLIENT: {
        RECT r; GetClientRect(hwnd, &r);
        Gdiplus::Graphics graphics(reinterpret_cast<HDC>(wParam));
        drawArtwork(graphics, r.right, r.bottom);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        pressed = hitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (pressed) SetCapture(hwnd);
        else { ReleaseCapture(); SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); }
        return 0;
    case WM_LBUTTONUP: {
        int action = hitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        int previous = pressed; pressed = 0; ReleaseCapture();
        if (previous && previous == action) activate(hwnd, action);
        return 0;
    }
    case WM_CAPTURECHANGED: pressed = 0; return 0;
    case WM_SETCURSOR: {
        POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
        if (hitTest(hwnd, p.x, p.y)) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) activate(hwnd, 2);
        if (wParam == VK_RETURN) activate(hwnd, 1);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Exercise the actual borderless window, its client rendering and button hit areas.
bool testWindow(const wchar_t* className) {
    for (int width : {artWidth, artWidth / 2}) {
        const int height = MulDiv(width, artHeight, artWidth);
        HWND hwnd = CreateWindowExW(0, className, L"D.A AURA Installer", WS_POPUP,
                                    0, 0, width, height, nullptr, nullptr, instance, nullptr);
        if (!hwnd) return false;
        RECT client = {}; GetClientRect(hwnd, &client);
        bool ok = client.right == width && client.bottom == height &&
                  hitTest(hwnd, MulDiv(1045, width, artWidth), MulDiv(571, height, artHeight)) == 1 &&
                  hitTest(hwnd, MulDiv(1462, width, artWidth), MulDiv(571, height, artHeight)) == 2 &&
                  hitTest(hwnd, MulDiv(1682, width, artWidth), MulDiv(38, height, artHeight)) == 3 &&
                  hitTest(hwnd, MulDiv(1620, width, artWidth), MulDiv(38, height, artHeight)) == 4 &&
                  hitTest(hwnd, MulDiv(400, width, artWidth), MulDiv(300, height, artHeight)) == 0;
        HDC dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dc || !dib || !bits) ok = false;
        HGDIOBJ old = dib ? SelectObject(dc, dib) : nullptr;
        if (ok) {
            SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
            Gdiplus::Bitmap expected(width, height, PixelFormat32bppARGB);
            { Gdiplus::Graphics graphics(&expected); drawArtwork(graphics, width, height); }
            for (int y : {40, 250, 420, 571, 630}) for (int x : {200, 500, 1045, 1462, 1650}) {
                const int dx = MulDiv(x, width, artWidth), dy = MulDiv(y, height, artHeight);
                Gdiplus::Color reference;
                COLORREF actual = GetPixel(dc, dx, dy);
                if (expected.GetPixel(dx, dy, &reference) != Gdiplus::Ok || actual == CLR_INVALID ||
                    std::abs(int(reference.GetRed()) - int(GetRValue(actual))) > 5 ||
                    std::abs(int(reference.GetGreen()) - int(GetGValue(actual))) > 5 ||
                    std::abs(int(reference.GetBlue()) - int(GetBValue(actual))) > 5) ok = false;
            }
        }
        if (old) SelectObject(dc, old);
        if (dib) DeleteObject(dib);
        if (dc) DeleteDC(dc);
        DestroyWindow(hwnd);
        if (!ok) return false;
    }
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR commandLine, int show) {
    instance = hInstance;
    if (commandLine && wcscmp(commandLine, L"--verify-payload") == 0) {
        for (const auto& file : payloadFiles) {
            HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(file.resourceId), RT_RCDATA);
            if (!resource || SizeofResource(instance, resource) == 0 || !LockResource(LoadResource(instance, resource))) return 1;
        }
        return 0;
    }
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 3 && wcscmp(argv[1], L"--test-install-root") == 0) {
        int result = 0;
        try { installPlugin(fs::path(argv[2])); } catch (...) { result = 1; }
        LocalFree(argv);
        return result;
    }
    if (argv) LocalFree(argv);

    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(100), RT_RCDATA);
    HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
    const BYTE* bytes = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    if (bytes) artworkStream = SHCreateMemStream(bytes, SizeofResource(instance, resource));
    if (artworkStream) artwork = Gdiplus::Bitmap::FromStream(artworkStream);
    if (!artwork || artwork->GetLastStatus() != Gdiplus::Ok) {
        MessageBoxW(nullptr, L"The installer artwork could not be loaded.", L"D.A AURA Installer", MB_OK | MB_ICONERROR);
        delete artwork; if (artworkStream) artworkStream->Release(); Gdiplus::GdiplusShutdown(token); return 1;
    }
    if (commandLine && wcscmp(commandLine, L"--test-render") == 0) {
        const bool ok = testRendering();
        delete artwork; artworkStream->Release(); Gdiplus::GdiplusShutdown(token);
        return ok ? 0 : 2;
    }
    WNDCLASSW wc = {}; wc.lpfnWndProc = windowProc; wc.hInstance = instance;
    wc.lpszClassName = L"DAAURAInstaller"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    if (commandLine && wcscmp(commandLine, L"--test-window") == 0) {
        const bool ok = testWindow(wc.lpszClassName);
        delete artwork; artworkStream->Release(); Gdiplus::GdiplusShutdown(token);
        return ok ? 0 : 3;
    }

    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int availW = std::max(320, static_cast<int>(work.right - work.left - 48));
    const int availH = std::max(200, static_cast<int>(work.bottom - work.top - 48));
    HDC screenDc = GetDC(nullptr);
    const int dpi = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSX) : 96;
    if (screenDc) ReleaseDC(nullptr, screenDc);

    int desiredW = (dpi >= 168) ? MulDiv(artWidth, dpi, 96) : artWidth;
    int width = std::min(desiredW, availW);
    int height = MulDiv(width, artHeight, artWidth);
    if (height > availH) {
        height = availH;
        width = MulDiv(height, artWidth, artHeight);
    }
    const int posX = work.left + (work.right - work.left - width) / 2;
    const int posY = work.top + (work.bottom - work.top - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_LAYERED, wc.lpszClassName, L"D.A AURA Installer",
        WS_POPUP | WS_MINIMIZEBOX, posX, posY, width, height, nullptr, nullptr, instance, nullptr);
    if (!hwnd) { delete artwork; artworkStream->Release(); Gdiplus::GdiplusShutdown(token); return 1; }

    layeredWindow = applyLayeredWindow(hwnd, posX, posY, width, height);
    if (!layeredWindow) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        int cut = std::max(1, MulDiv(cornerCut, width, artWidth));
        POINT points[] = {{cut,0},{width-cut,0},{width,cut},{width,height-cut},{width-cut,height},{cut,height},{0,height-cut},{0,cut}};
        HRGN region = CreatePolygonRgn(points, 8, WINDING);
        if (region && !SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
    }

    ShowWindow(hwnd, show);
    if (!layeredWindow) UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    delete artwork; artworkStream->Release(); Gdiplus::GdiplusShutdown(token);
    return 0;
}
