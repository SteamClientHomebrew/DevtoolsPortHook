# Praesidium Sentinel

This project is sentinel for Praesidium. It randomized port selection of the Chrome Developer Tools, and enabled CORS.  

## How does it work?

To hook into Steam's process flow, we place our custom `version.dll` directly into the Steam installation directory. This works due to the way Windows performs DLL resolution. When an application like Steam attempts to load a system DLL such as version.dll, the Windows loader follows a specific search order. Crucially, it prioritizes the current working directory (in this case, the Steam folder) before checking `C:\Windows\System32`.

This behavior is rooted in optimization. Searching the current directory, which typically contains fewer files, can be faster than scanning the much larger System32 directory. As a result, if `version.dll` exists in Steam's directory, it will be loaded instead of the legitimate system one.

Once our custom DLL is loaded in place of the real version.dll, we must ensure that Steam continues to function normally.
This is where DLL proxying comes into play. DLL proxying allows our custom DLL to act as a middleman: intercepting calls and optionally modifying behavior, but still forwarding legitimate function calls to the real system DLL.

Using MinGW, we can implement proxying without static linking. For each function we want to forward, such as GetFileVersionInfo, we load the real `version.dll` from System32 at runtime and forward the call accordingly.
`GetFileVersionInfoA="C:\\Windows\\System32\\version".GetFileVersionInfoA`

By implementing this pattern for each exported function, we can safely intercept or observe behavior while preserving the functionality expected by Steam.
