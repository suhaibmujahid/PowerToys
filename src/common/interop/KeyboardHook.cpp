#include "pch.h"
#include "KeyboardHook.h"
#include <exception>

using namespace interop;
using namespace System::Runtime::InteropServices;

KeyboardHook::KeyboardHook()
{
    // initialize thread
    // initialize window
    kbEventDispatch = gcnew Thread(gcnew ThreadStart(this, &KeyboardHook::DispatchProc));
    queue = gcnew Queue<ShortcutCallback ^>();
    shortcuts = gcnew Dictionary<SHORTCUT_HANDLE, ShortcutCallback ^>();
    pressedKeys = gcnew Shortcut();
}

KeyboardHook::~KeyboardHook()
{
    quit = true;
    kbEventDispatch->Join();

    // Unregister low level hook procedure
    UnhookWindowsHookEx(hookHandle);
}

void KeyboardHook::DispatchProc()
{
    Monitor::Enter(queue);
    quit = false;
    while (!quit)
    {
        if (queue->Count == 0)
        {
            Monitor::Wait(queue);
            continue;
        }
        auto nextCallback = queue->Dequeue();

        // Release lock while callback is being invoked
        Monitor::Exit(queue);
        
        nextCallback->Invoke();
        
        // Re-aquire lock
        Monitor::Enter(queue);
    }

    Monitor::Exit(queue);
}

SHORTCUT_HANDLE KeyboardHook::GetHookHandle(Shortcut ^ shortcut)
{
    SHORTCUT_HANDLE handle = shortcut->Key;
    handle |= shortcut->Win << 8;
    handle |= shortcut->Ctrl << 9;
    handle |= shortcut->Shift << 10;
    handle |= shortcut->Alt << 11;
    return handle;
}

SHORTCUT_HANDLE KeyboardHook::RegisterShortcut(
    Shortcut ^ shortcut,
    ShortcutCallback ^ callback)
{
    SHORTCUT_HANDLE handle = GetHookHandle(shortcut);
    shortcuts->Add(handle, callback);
    return handle;
}

bool KeyboardHook::UnregisterShortcut(SHORTCUT_HANDLE handle)
{
    return shortcuts->Remove(handle);
}

void KeyboardHook::UpdatePressedKey(DWORD code, bool replaceWith, unsigned char replaceWithKey)
{
    switch (code)
    {
    case VK_LWIN:
    case VK_RWIN:
        pressedKeys->Win = replaceWith;
        break;
    case VK_CONTROL:
        pressedKeys->Ctrl = replaceWith;
        break;
    case VK_SHIFT:
        pressedKeys->Shift = replaceWith;
        break;
    case VK_MENU:
        pressedKeys->Alt = replaceWith;
        break;
    default:
        pressedKeys->Key = replaceWithKey;
        break;
    }
}

void KeyboardHook::UpdatePressedKeys(KeyboardEvent ^ ev)
{
    switch (ev->wParam)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        UpdatePressedKey(ev->lParam->vkCode, true, ev->lParam->vkCode);
    }
    break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        UpdatePressedKey(ev->lParam->vkCode, false, 0);
    }
    break;
    }
}

void KeyboardHook::Start()
{
    auto del = gcnew HookProcDelegate(this, &KeyboardHook::HookProc);
    // register low level hook procedure
    hookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) (void*) Marshal::GetFunctionPointerForDelegate(del), 0, 0);
    if (hookHandle == nullptr)
    {
        throw std::exception("SetWindowsHookEx failed.");
    }
}

LRESULT CALLBACK KeyboardHook::HookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KeyboardEvent ^ ev = gcnew KeyboardEvent();
        ev->wParam = wParam;
        ev->lParam = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        UpdatePressedKeys(ev);
        SHORTCUT_HANDLE pressedKeysHookHandle = GetHookHandle(pressedKeys);
        if (shortcuts->ContainsKey(pressedKeysHookHandle))
        {
            Monitor::Enter(queue);
            queue->Enqueue(shortcuts[pressedKeysHookHandle]);
            Monitor::Exit(queue);
            return 1;
        }
    }
    return CallNextHookEx(hookHandle, nCode, wParam, lParam);
}