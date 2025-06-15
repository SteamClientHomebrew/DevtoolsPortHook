## Devtools Port Hook

This project is not currently used within Millennium. It is simply a POC that shows this _is_ possible.

### How does it work?

We put our `version.dll` into the Steam path. Due to how DLL lookups work under the hood on windows, when Steam tries to load `version.dll` from System32, it will attempt to load matching DLL's from the current working directory before searching System32.
This is in place to make DLL lookup quicker as it favors the current working directory (which likely has far less DLL's in it) over System32, prioritizing a potential faster solution.

Now that we've established we can proxy our own DLL from the Steam directory, we need a way to forward legit calls from Steam to the actual System32 `version.dll` to ensure it still actually functions. We use a process called DLL proxying, and in MinGW, we can easily forward functions from our DLL to an external one (without static linking)

In this example, we forward `GetFileVersionInfoA` calls from our DLL, to the real System32 `version.dll`
`GetFileVersionInfoA="C:\\Windows\\System32\\version".GetFileVersionInfoA`
