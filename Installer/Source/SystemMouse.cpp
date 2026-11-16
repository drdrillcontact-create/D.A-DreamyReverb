// For the installer's self-test only (--self-test): moves the mouse pointer and presses and
// releases its button through the system, the way a hand on a mouse does, so that the window
// is tried the way it will be used. Kept apart from the rest so that the system's headers and
// the JUCE headers never meet.
//
//   action   0 = move, 1 = press, 2 = release, 3 = move with the button held down
//   x, y     where, counted from the top left of the main screen (Windows: pixels, macOS: points)
//
// Returns 1 if the system took the event, 0 if it cannot be done here.

extern "C" int daaura_postMouse (int action, int x, int y);

#if defined (_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>

extern "C" int daaura_postMouse (int action, int x, int y)
{
    const int width  = GetSystemMetrics (SM_CXSCREEN);
    const int height = GetSystemMetrics (SM_CYSCREEN);

    if (width < 2 || height < 2)
        return 0;

    // (absolute positions run from 0 to 65535 across the main screen)
    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG> ((static_cast<long long> (x) * 65535ll + (width  - 1) / 2) / (width  - 1));
    input.mi.dy = static_cast<LONG> ((static_cast<long long> (y) * 65535ll + (height - 1) / 2) / (height - 1));
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE
                     | (action == 1 ? MOUSEEVENTF_LEFTDOWN : 0)
                     | (action == 2 ? MOUSEEVENTF_LEFTUP : 0);

    return SendInput (1, &input, sizeof (INPUT)) == 1 ? 1 : 0;
}

#elif defined (__APPLE__)

#include <ApplicationServices/ApplicationServices.h>

extern "C" int daaura_postMouse (int action, int x, int y)
{
    // (macOS only lets a program do this that has been allowed to; it is not asked for here)
    if (! CGPreflightPostEventAccess())
        return 0;

    const CGEventType type = action == 1 ? kCGEventLeftMouseDown
                           : action == 2 ? kCGEventLeftMouseUp
                           : action == 3 ? kCGEventLeftMouseDragged
                                         : kCGEventMouseMoved;

    const CGEventRef event = CGEventCreateMouseEvent (nullptr, type, CGPointMake (static_cast<CGFloat> (x), static_cast<CGFloat> (y)),
                                                      kCGMouseButtonLeft);

    if (event == nullptr)
        return 0;

    CGEventPost (kCGHIDEventTap, event);
    CFRelease (event);
    return 1;
}

#else

extern "C" int daaura_postMouse (int, int, int)
{
    return 0;
}

#endif
