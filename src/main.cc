#include <windows.h>
#include <winternl.h>
#include <iostream>
#include "MinHook.h"

typedef struct _LDR_DLL_NOTIFICATION_DATA {
    ULONG Flags;
    PCUNICODE_STRING FullDllName;
    PCUNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
} 
DR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK *PLDR_DLL_NOTIFICATION_FUNCTION)(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context);
typedef NTSTATUS (NTAPI *LdrRegisterDllNotification_t)(ULONG Flags, PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction, PVOID Context, PVOID *Cookie);
typedef NTSTATUS (NTAPI *LdrUnregisterDllNotification_t)(PVOID Cookie);

#define LDR_DLL_NOTIFICATION_REASON_LOADED   1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

LdrRegisterDllNotification_t LdrRegisterDllNotification = nullptr;
LdrUnregisterDllNotification_t LdrUnregisterDllNotification = nullptr;
PVOID g_NotificationCookie = nullptr;

typedef const char* (__stdcall* Plat_CommandLineParamValue_t)(const char* param);

Plat_CommandLineParamValue_t fpOriginalPlat_CommandLineParamValue = nullptr;

const char* __stdcall Hooked_Plat_CommandLineParamValue(const char* param) {
    if (strcmp(param, "-devtools-port") == 0) 
        return "9222";
    
    return fpOriginalPlat_CommandLineParamValue(param);
}

VOID HandleTier0Dll(PVOID moduleBaseAddress) {
    if (MH_Initialize() != MH_OK) 
        return;
    
    HMODULE hModule = (HMODULE)moduleBaseAddress;
    FARPROC pFunc = GetProcAddress(hModule, "Plat_CommandLineParamValue");

    if (pFunc == nullptr) 
        return;
    
    if (MH_CreateHook(reinterpret_cast<LPVOID>(pFunc), reinterpret_cast<LPVOID>(&Hooked_Plat_CommandLineParamValue), reinterpret_cast<LPVOID*>(&fpOriginalPlat_CommandLineParamValue)) != MH_OK) 
        return;
    
    if (MH_EnableHook(reinterpret_cast<LPVOID>(pFunc)) != MH_OK) 
        return;
}

VOID CALLBACK DllNotificationCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context) {
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED) {
        std::wstring dllName(
            NotificationData->BaseDllName->Buffer,
            NotificationData->BaseDllName->Length / sizeof(WCHAR)
        );
        
        if (dllName == L"tier0_s.dll") {
            HandleTier0Dll(NotificationData->DllBase);
        }
    }
}

bool InitializeDllNotification() {
    HMODULE ntdllModule = GetModuleHandleA("ntdll.dll");
    if (!ntdllModule) 
        return false;
    
    LdrRegisterDllNotification = (LdrRegisterDllNotification_t)GetProcAddress(ntdllModule, "LdrRegisterDllNotification");
    LdrUnregisterDllNotification = (LdrUnregisterDllNotification_t)GetProcAddress(ntdllModule, "LdrUnregisterDllNotification");
    
    if (!LdrRegisterDllNotification || !LdrUnregisterDllNotification) 
        return false;
    
    return NT_SUCCESS(LdrRegisterDllNotification(0, DllNotificationCallback, nullptr, &g_NotificationCookie));
}

void CleanupDllNotification() {
    if (g_NotificationCookie && LdrUnregisterDllNotification) {
        LdrUnregisterDllNotification(g_NotificationCookie);
        g_NotificationCookie = nullptr;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        InitializeDllNotification();
        break;
    case DLL_PROCESS_DETACH:
        CleanupDllNotification();
        break;
    }
    return TRUE;
}