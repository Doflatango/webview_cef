// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "client_app.h"

#include "include/cef_command_line.h"

namespace {

// These flags must match the Chromium values.
const char kProcessType[] = "type";
const char kRendererProcess[] = "renderer";
#if defined(OS_LINUX)
const char kZygoteProcess[] = "zygote";
#endif

}  // namespace

ClientApp::ClientApp() {}

// static
ClientApp::ProcessType ClientApp::GetProcessType(
    CefRefPtr<CefCommandLine> command_line) {
  // The command-line flag won't be specified for the browser process.
  if (!command_line->HasSwitch(kProcessType))
    return BrowserProcess;

  const std::string& process_type = command_line->GetSwitchValue(kProcessType);
  if (process_type == kRendererProcess)
    return RendererProcess;
#if defined(OS_LINUX)
  else if (process_type == kZygoteProcess)
    return ZygoteProcess;
#endif

  return OtherProcess;
}

// void ClientApp::OnRegisterCustomSchemes(
//     CefRawPtr<CefSchemeRegistrar> registrar) {
//   RegisterCustomSchemes(registrar);
// }
