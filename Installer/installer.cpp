#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <cwchar>
#include <cstdlib>
#include <initializer_list>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include "payload.h"

namespace fs = std::filesystem;
namespace {
constexpr int artWidth = 1466, artHeight = 560;
HINSTANCE instance;
Gdiplus::Bitmap* artwork = nullptr;
IStream* artworkStream = nullptr;
int pressed = 0;
bool installing = false;

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
    winCheck(GetTempFileNameW(directory.c_str(), L"DAR", 0, name) != 0, "Cannot create temporary path");
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

fs::path installPlugin() {
    PWSTR common = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, nullptr, &common);
    if (FAILED(hr)) throw std::runtime_error("Cannot find the common Program Files folder");
    fs::path directory = fs::path(common) / L"VST3";
    CoTaskMemFree(common);
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
            if (restoreError) throw std::runtime_error("Installation failed and the previous version could not be restored. The previous version is preserved in a DAR temporary folder in the VST3 directory.");
        }
        throw;
    }
    if (oldMoved) {
        std::error_code ignored;
        fs::remove_all(backup, ignored);
    }
    return target;
}

// Hit areas are expressed in coordinates of the reference artwork.
int hitTest(HWND hwnd, int x, int y) {
    RECT r; GetClientRect(hwnd, &r);
    if (r.right == 0 || r.bottom == 0) return 0;
    x = MulDiv(x, artWidth, r.right); y = MulDiv(y, artHeight, r.bottom);
    if (x >= 716 && x <= 1049 && y >= 453 && y <= 514) return 1; // Next
    if (x >= 1081 && x <= 1389 && y >= 453 && y <= 514) return 2; // Cancel
    if (x >= 1396 && x <= 1440 && y >= 5 && y <= 60) return 3; // Close
    if (x >= 1344 && x < 1396 && y >= 5 && y <= 60) return 4; // Minimize
    return 0;
}

void activate(HWND hwnd, int action) {
    if (installing) return;
    if (action == 2 || action == 3) { DestroyWindow(hwnd); return; }
    if (action == 4) { ShowWindow(hwnd, SW_MINIMIZE); return; }
    if (action != 1) return;
    if (MessageBoxW(hwnd, L"Install D.A AURA to the standard VST3 folder?\n\nThe installer contains the complete plugin.", L"D.A AURA", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    installing = true;
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    try {
        fs::path target = installPlugin();
        std::wstring message = L"Installation complete:\n\n" + target.wstring() + L"\n\nRestart or rescan your DAW to find the plugin.";
        MessageBoxW(hwnd, message.c_str(), L"D.A AURA", MB_OK | MB_ICONINFORMATION);
        DestroyWindow(hwnd);
    } catch (const std::exception& e) {
        std::wstring message = L"Installation failed. No new plugin was installed.\n\n" + errorText(e);
        MessageBoxW(hwnd, message.c_str(), L"D.A AURA", MB_OK | MB_ICONERROR);
        installing = false;
    }
}

// Explicit pixel units and a source rectangle prevent GDI+ from applying the
// bitmap/screen DPI ratio a second time on high-DPI displays.
void drawArtwork(Gdiplus::Graphics& graphics, int width, int height) {
    graphics.SetPageUnit(Gdiplus::UnitPixel);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(artwork, Gdiplus::Rect(0, 0, width, height),
                       0, 0, artWidth, artHeight, Gdiplus::UnitPixel);
}

bool testRendering() {
    const auto diff = [](BYTE a, BYTE b) { return std::abs(int(a) - int(b)); };
    for (int width : {artWidth, artWidth / 2}) {
        const int height = MulDiv(width, artHeight, artWidth);
        Gdiplus::Bitmap baseline(width, height, PixelFormat32bppARGB);
        baseline.SetResolution(96.0f, 96.0f);
        { Gdiplus::Graphics graphics(&baseline); drawArtwork(graphics, width, height); }
        // The right/bottom areas (including buttons) must really be drawn.
        if (width == artWidth) {
            for (int y : {30, 200, 350, 480, 530}) {
                for (int x : {100, 500, 800, 1200, 1400}) {
                    Gdiplus::Color expected, actual;
                    if (artwork->GetPixel(x, y, &expected) != Gdiplus::Ok ||
                        baseline.GetPixel(x, y, &actual) != Gdiplus::Ok) return false;
                    if (diff(expected.GetRed(), actual.GetRed()) > 65 ||
                        diff(expected.GetGreen(), actual.GetGreen()) > 65 ||
                        diff(expected.GetBlue(), actual.GetBlue()) > 65) return false;
                }
            }
        }
        // The same client size must render identically at higher target DPIs.
        for (float dpi : {120.0f, 144.0f, 192.0f}) {
            Gdiplus::Bitmap target(width, height, PixelFormat32bppARGB);
            if (target.SetResolution(dpi, dpi) != Gdiplus::Ok) return false;
            { Gdiplus::Graphics graphics(&target); drawArtwork(graphics, width, height); }
            for (int y : {30, 200, 350, 480, 530}) {
                for (int x : {100, 500, 800, 1200, 1400}) {
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
        RECT r; GetClientRect(hwnd, &r);
        Gdiplus::Graphics graphics(dc);
        drawArtwork(graphics, r.right, r.bottom);
        EndPaint(hwnd, &ps); return 0;
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
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR commandLine, int show) {
    instance = hInstance;
    // Used by CI after copying the executable away from the build and staging folders.
    if (commandLine && wcscmp(commandLine, L"--verify-payload") == 0) {
        for (const auto& file : payloadFiles) {
            HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(file.resourceId), RT_RCDATA);
            if (!resource || SizeofResource(instance, resource) == 0 || !LockResource(LoadResource(instance, resource))) return 1;
        }
        return 0;
    }
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(100), RT_RCDATA);
    HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
    const BYTE* bytes = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    if (bytes) artworkStream = SHCreateMemStream(bytes, SizeofResource(instance, resource));
    if (artworkStream) artwork = Gdiplus::Bitmap::FromStream(artworkStream);
    if (!artwork || artwork->GetLastStatus() != Gdiplus::Ok) {
        MessageBoxW(nullptr, L"The installer artwork could not be loaded.", L"D.A AURA", MB_OK | MB_ICONERROR);
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
    RECT work; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int width = std::min(artWidth, std::max(1, static_cast<int>(work.right - work.left - 32)));
    int height = MulDiv(width, artHeight, artWidth);
    if (height > work.bottom - work.top - 32) {
        height = std::max(1, static_cast<int>(work.bottom - work.top - 32)); width = MulDiv(height, artWidth, artHeight);
    }
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"D.A AURA Installer", WS_POPUP | WS_MINIMIZEBOX,
        work.left + (work.right - work.left - width) / 2, work.top + (work.bottom - work.top - height) / 2,
        width, height, nullptr, nullptr, instance, nullptr);
    if (!hwnd) { delete artwork; artworkStream->Release(); Gdiplus::GdiplusShutdown(token); return 1; }
    // Match the clipped corners of the supplied design, without a native title bar.
    int cut = std::max(1, MulDiv(17, width, artWidth));
    POINT points[] = {{cut,0},{width-cut,0},{width,cut},{width,height-cut},{width-cut,height},{cut,height},{0,height-cut},{0,cut}};
    HRGN region = CreatePolygonRgn(points, 8, WINDING);
    if (region && !SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
    ShowWindow(hwnd, show); UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    delete artwork; artworkStream->Release(); Gdiplus::GdiplusShutdown(token);
    return 0;
}
