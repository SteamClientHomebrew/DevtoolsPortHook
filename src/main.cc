/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>
#define _WINSOCKAPI_
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <asio.hpp>
#include <asio/ip/tcp.hpp>
#include "MinHook.h"
#include <bits/this_thread_sleep.h>
#define LDR_DLL_NOTIFICATION_REASON_LOADED   1
#define DEFAULT_DEVTOOLS_PORT "8080"

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

LdrRegisterDllNotification_t LdrRegisterDllNotification = nullptr;
LdrUnregisterDllNotification_t LdrUnregisterDllNotification = nullptr;
PVOID g_NotificationCookie = nullptr;

typedef LPCSTR (__attribute__((__cdecl__))* Plat_CommandLineParamValue_t)(LPCSTR param);
typedef CHAR (__attribute__((__cdecl__))* Plat_CommandLineParamExists_t)(LPCSTR param); 

// No idea why then made this return a char? boolean and char are the same size on x86/x64, so this is useless.
typedef INT (__attribute__((__cdecl__))* CreateSimpleProcess_t)(LPCCH commandLine, CHAR arg2);

Plat_CommandLineParamValue_t fpOriginalPlat_CommandLineParamValue = nullptr;
Plat_CommandLineParamExists_t fpPlat_CommandLineParamExists = nullptr;
CreateSimpleProcess_t fpCreateSimpleProcess = nullptr;

asio::ip::port_type GetRandomOpenPort() {
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor(io_context);
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), 0);

    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen();
    return acceptor.local_endpoint().port();
}

HMODULE steamTier0Module;

LPCSTR Hooked_Plat_CommandLineParamValue(LPCSTR param) {
    if (strcmp(param, "-devtools-port") == 0) {
        static const std::string RANDOM_OPEN_PORT = std::to_string(GetRandomOpenPort());

        FARPROC pFunc = GetProcAddress(steamTier0Module, "Plat_CommandLineParamExists");
        if (pFunc == nullptr) return RANDOM_OPEN_PORT.c_str();

        fpPlat_CommandLineParamExists = reinterpret_cast<Plat_CommandLineParamExists_t>(pFunc);
        if (fpPlat_CommandLineParamExists == nullptr || !fpPlat_CommandLineParamExists("-dev")) return RANDOM_OPEN_PORT.c_str();
        
        return DEFAULT_DEVTOOLS_PORT; // Default port if -dev is present
    }
    return fpOriginalPlat_CommandLineParamValue(param);
}

LPCSTR SecurityBlockRemoteOrigins(LPCSTR cmd) {
    if (!cmd) return nullptr;
    std::string input(cmd);

    size_t start = input.find('"');
    if (start == std::string::npos) return strdup(cmd); 

    size_t end = input.find('"', start + 1);
    if (end == std::string::npos) return strdup(cmd);

    std::string exePath = input.substr(start + 1, end - start - 1);
    std::string exePathLower = exePath;
    for (auto& c : exePathLower) c = static_cast<char>(tolower(c));

    if (exePathLower.find("steamwebhelper.exe") == std::string::npos) {
        PCHAR out = new CHAR[input.size() + 1];
        std::strcpy(out, cmd);
        return out;
    }

    std::string target = "--remote-allow-origins=*";
    size_t pos = input.find(target);
    if (pos != std::string::npos) {
        input.replace(pos, target.size(), R"(--remote-allow-origins=\"\")");
    }

    PCHAR out = new CHAR[input.size() + 1];
    std::strcpy(out, input.c_str());
    return out;
}

INT Hooked_CreateSimpleProcess(LPCCH commandLine, CHAR arg2) {
    if (fpPlat_CommandLineParamExists != nullptr && !fpPlat_CommandLineParamExists("-dev"))
        commandLine = SecurityBlockRemoteOrigins(commandLine);
    
    return fpCreateSimpleProcess(commandLine, arg2);
}

VOID HandleTier0Dll(PVOID moduleBaseAddress) {
    if (MH_Initialize() != MH_OK) return;
    
    steamTier0Module = (HMODULE)moduleBaseAddress;
    FARPROC Plat_CommandLineParamValue = GetProcAddress(steamTier0Module, "Plat_CommandLineParamValue");
    FARPROC CreateSimpleProcess = GetProcAddress(steamTier0Module, "CreateSimpleProcess");

    if (Plat_CommandLineParamValue != nullptr) 
        if (MH_CreateHook(reinterpret_cast<LPVOID>(Plat_CommandLineParamValue), reinterpret_cast<LPVOID>(&Hooked_Plat_CommandLineParamValue), reinterpret_cast<LPVOID*>(&fpOriginalPlat_CommandLineParamValue)) != MH_OK) return;

    if (CreateSimpleProcess != nullptr) 
        if (MH_CreateHook(reinterpret_cast<LPVOID>(CreateSimpleProcess), reinterpret_cast<LPVOID>(&Hooked_CreateSimpleProcess), reinterpret_cast<LPVOID*>(&fpCreateSimpleProcess)) != MH_OK) return;

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) return;
}

VOID CALLBACK DllNotificationCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context) {
    if (NotificationReason != LDR_DLL_NOTIFICATION_REASON_LOADED)
        return;

    if (std::wstring(NotificationData->BaseDllName->Buffer, NotificationData->BaseDllName->Length / sizeof(WCHAR)) == L"tier0_s.dll") 
        HandleTier0Dll(NotificationData->DllBase);
}

BOOL InitializeDllNotification() {
    HMODULE ntdllModule = GetModuleHandleA("ntdll.dll");
    if (!ntdllModule) return false;
    
    LdrRegisterDllNotification   = (LdrRegisterDllNotification_t)GetProcAddress(ntdllModule, "LdrRegisterDllNotification");
    LdrUnregisterDllNotification = (LdrUnregisterDllNotification_t)GetProcAddress(ntdllModule, "LdrUnregisterDllNotification");
    
    if (!LdrRegisterDllNotification || !LdrUnregisterDllNotification) return false;
    return NT_SUCCESS(LdrRegisterDllNotification(0, DllNotificationCallback, nullptr, &g_NotificationCookie));
}

VOID CleanupDllNotification() {
    if (g_NotificationCookie && LdrUnregisterDllNotification) {
        LdrUnregisterDllNotification(g_NotificationCookie);
        g_NotificationCookie = nullptr;
    }
}

BOOL IsSteamClient() {
    CHAR p[MAX_PATH], *n;
    GetModuleFileNameA(0, p, MAX_PATH);
    n = strrchr(p, '\\');
    return !_stricmp(n ? n + 1 : p, "steam.exe");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (!IsSteamClient()) return TRUE; // Not a Steam client, no need to proceed
    
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: InitializeDllNotification(); break;
        case DLL_PROCESS_DETACH: CleanupDllNotification();    break;
    }
    return TRUE;
}