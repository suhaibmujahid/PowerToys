#pragma once

using namespace System::Threading;
using namespace System::Collections::Generic;

namespace interop
{
public
    delegate void ShortcutCallback();
public
    ref struct Shortcut
    {
        bool Win;
        bool Ctrl;
        bool Shift;
        bool Alt;
        unsigned char Key;
    };

    // bit 0-7: Virtual Key
    // bit 8-11: Win, Ctrl, Shift, Alt
    typedef unsigned short SHORTCUT_HANDLE;

private
    ref struct KeyboardEvent
    {
        WPARAM wParam;
        KBDLLHOOKSTRUCT* lParam;
    };

public
    ref class KeyboardHook
    {
    public:
        KeyboardHook();
        ~KeyboardHook();

        SHORTCUT_HANDLE RegisterShortcut(Shortcut ^ shortcut, ShortcutCallback ^ callback);
        bool UnregisterShortcut(SHORTCUT_HANDLE handle);
        void Start();
    private:
        delegate LRESULT HookProcDelegate(int nCode, WPARAM wParam, LPARAM lParam);
        Thread ^ kbEventDispatch;
        Queue<ShortcutCallback ^> ^ queue;
        Dictionary<SHORTCUT_HANDLE, ShortcutCallback ^> ^ shortcuts;
        Shortcut ^ pressedKeys;
        bool quit;
        HHOOK hookHandle;

        void DispatchProc();
        void UpdatePressedKeys(KeyboardEvent ^ ev);
        void UpdatePressedKey(DWORD virtualKey, bool replaceWith, unsigned char replaceWithKey);
        LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);
        static SHORTCUT_HANDLE GetHookHandle(Shortcut ^ shortcut);
    };

}
