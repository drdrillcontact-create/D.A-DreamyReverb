// The installer's window as Windows sees it. On the screen there is to be the design's window
// and nothing else, so the window is see-through around the frame it draws, and has neither
// border nor shadow of the system's. What follows from that on Windows is put right here.
// (macOS needs nothing of the kind.) Kept apart from the rest so that the system's headers
// and the JUCE headers never meet.
//
//   nativeWindowHandle   the window, as ComponentPeer::getNativeHandle() gives it

extern "C" void daaura_plainWindowEdge (void* nativeWindowHandle);

#if defined (_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

#if defined (_MSC_VER)
 #pragma comment (lib, "dwmapi.lib")
#endif

extern "C" void daaura_plainWindowEdge (void* nativeWindowHandle)
{
    if (nativeWindowHandle == nullptr)
        return;

    const HWND window = static_cast<HWND> (nativeWindowHandle);

    // A see-through window is made as a bare pop-up. The taskbar only minimises a window, and
    // only offers to close it, when the window says that it can be: so it says so.
    const LONG_PTR style = GetWindowLongPtr (window, GWL_STYLE);

    if (style != 0)
        SetWindowLongPtr (window, GWL_STYLE, style | WS_SYSMENU | WS_MINIMIZEBOX);

    // Both are settings of Windows 11: no line of the system's around the window, no rounding
    // of its corners. (An older Windows answers that it does not know them: it has neither.)
    const DWORD noLine = 0xFFFFFFFE;        // DWMWA_COLOR_NONE
    DwmSetWindowAttribute (window, 34 /* DWMWA_BORDER_COLOR */, &noLine, sizeof (noLine));

    const DWORD square = 1;                 // DWMWCP_DONOTROUND
    DwmSetWindowAttribute (window, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &square, sizeof (square));
}

#else

extern "C" void daaura_plainWindowEdge (void*)
{
}

#endif
