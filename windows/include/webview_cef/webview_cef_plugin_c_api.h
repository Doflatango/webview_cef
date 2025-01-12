#ifndef FLUTTER_PLUGIN_WEBVIEW_CEF_PLUGIN_C_API_H_
#define FLUTTER_PLUGIN_WEBVIEW_CEF_PLUGIN_C_API_H_

#include <windows.h>
#include <flutter_plugin_registrar.h>

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FLUTTER_PLUGIN_EXPORT __declspec(dllimport)
#endif

#if defined(__cplusplus)
extern "C" {
#endif

FLUTTER_PLUGIN_EXPORT void WebviewCefPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar);

FLUTTER_PLUGIN_EXPORT int InitCEFProcesses();

FLUTTER_PLUGIN_EXPORT void ProcessMessageForCEF(unsigned int message, unsigned __int64 wParam, __int64 lParam);

void processKeyEventForCEF(unsigned int message, unsigned __int64 wParam, __int64 lParam);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FLUTTER_PLUGIN_WEBVIEW_CEF_PLUGIN_C_API_H_
