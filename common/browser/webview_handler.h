// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef COMMON_BROWSER_WEBVIEW_HANDLER_H_
#define COMMON_BROWSER_WEBVIEW_HANDLER_H_
#pragma once

#include "include/cef_client.h"
#include "include/wrapper/cef_message_router.h"
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>
#include <flutter/binary_messenger.h>
#include <flutter/event_channel.h>
#include <flutter/method_result.h>

#include <functional>

namespace
{

constexpr auto kEventType = "type";
constexpr auto kEventValue = "value";

constexpr auto kEventTitleChanged = "titleChanged";
constexpr auto kEventURLChanged = "urlChanged";
constexpr auto kEventCursorChanged = "cursorChanged";
constexpr auto kEventLoadingProgressChanged = "loadingProgressChanged";
constexpr auto kEventScrollOffsetChanged = "scrollOffsetChanged";
constexpr auto kEventLoadingStateChanged = "loadingStateChanged";
constexpr auto kEventLoadStart = "loadStart";
constexpr auto kEventLoadEnd = "loadEnd";
constexpr auto kEventLoadError = "loadError";
constexpr auto kEventIMEComposionPositionChanged = "imeComposionPositionChanged";
constexpr auto kEventAsyncChannelMessage = "asyncChannelMessage";

constexpr auto kErrorInvalidArguments = "InvalidArguments";

}

class WebviewHandler : public CefClient,
public CefDisplayHandler,
public CefLifeSpanHandler,
public CefLoadHandler,
public CefRenderHandler,
public CefRequestHandler {
public:
    std::function<void(const void*, int32_t width, int32_t height)> onPaintCallback;
    std::function<void()> onBrowserClose;
    std::function<void (CefRefPtr<CefBrowser> browser,
                        const CefRange& selection_range,
                        const CefRenderHandler::RectList& character_bounds)> onImeCompositionRangeChangedCallback;

    explicit WebviewHandler(flutter::BinaryMessenger* messenger, int browser_id, bool headless);
    ~WebviewHandler();

    // CefClient methods:
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefProcessId source_process,
                                          CefRefPtr<CefProcessMessage> message) override;

    // CefDisplayHandler methods:
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                               const CefString& title) override;
    virtual void OnAddressChange(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& url) override;
    virtual bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                                CefCursorHandle cursor,
                                cef_cursor_type_t type,
                                const CefCursorInfo& custom_cursor_info) override;
    virtual void OnLoadingProgressChange(CefRefPtr<CefBrowser> browser,
                                       double progress) override;

    virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                      bool isLoading,
                                      bool canGoBack,
                                      bool canGoForward) override;
    virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             TransitionType transition_type) override;

    virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int httpStatusCode) override;

    // CefLifeSpanHandler methods:
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    // virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
    //                            CefRefPtr<CefFrame> frame,
    //                            const CefString& target_url,
    //                            const CefString& target_frame_name,
    //                            CefLifeSpanHandler::WindowOpenDisposition target_disposition,
    //                            bool user_gesture,
    //                            const CefPopupFeatures& popupFeatures,
    //                            CefWindowInfo& windowInfo,
    //                            CefRefPtr<CefClient>& client,
    //                            CefBrowserSettings& settings,
    //                            CefRefPtr<CefDictionaryValue>& extra_info,
    //                            bool* no_javascript_access) override;
    
    // CefLoadHandler methods:
    virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode,
                             const CefString& errorText,
                             const CefString& failedUrl) override;
    
    // CefRenderHandler methods:
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;
    virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) override;
    virtual bool StartDragging(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefDragData> drag_data,
                               DragOperationsMask allowed_ops,
                               int x,
                               int y) override;
    virtual void OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser,
                                       double x,
                                       double y) override;
    virtual void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                                              const CefRange& selection_range,
                                              const CefRenderHandler::RectList& character_bounds) override;


    // CefRequestHandler methods:
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override;
    void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                   TerminationStatus status) override;

    // Request that all existing browser windows close.
    void CloseAllBrowsers(bool force_close);

    // Returns true if the Chrome runtime is enabled.
    static bool IsChromeRuntimeEnabled();

    void sendScrollEvent(int x, int y, int deltaX, int deltaY);
    void changeSize(float a_dpi, int width, int height);
    void updateViewOffset(int x, int y);
    void cursorClick(int x, int y, bool up);
    void cursorMove(int x, int y, bool dragging);
    void sendKeyEvent(CefKeyEvent ev);
    void loadUrl(std::string url);
    std::string getUrl();
    bool canGoForward();
    void goForward();
    bool canGoBack();
    void goBack();
    void reload();
    void stopLoad();
    void openDevTools();

    void PrintToPDF(std::string path, const CefPdfPrintSettings& settings, std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

    static const CefRefPtr<CefBrowser> CurrentFocusedBrowser();

private:
    uint32_t width_ = 1;
    uint32_t height_ = 1;
    int x_ = 0;
    int y_ = 0;
    float dpi_ = 1.0;
    bool is_dragging_ = false;
    bool is_focused_ = false;
    CefRect _prevIMEPosition = CefRect();

    CefRefPtr<CefBrowser> browser_;
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> browser_channel_;
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;
    std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> event_channel_;

    // Handles the browser side of query routing.
    CefRefPtr<CefMessageRouterBrowserSide> message_router_;
    std::unique_ptr<CefMessageRouterBrowserSide::Handler> message_handler_;

    void Focus();
    void Unfocus();

    void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

    template <typename T>
    void EmitEvent(const std::string eventType, const T& value) {
        if (event_sink_) {
            const auto event = flutter::EncodableValue(flutter::EncodableMap{
                {flutter::EncodableValue(kEventType), flutter::EncodableValue(eventType)},
                {flutter::EncodableValue(kEventValue), flutter::EncodableValue(value)},
            });
            event_sink_->Success(event);
        }
    }

    void EmitAsyncChannelMessage(const flutter::EncodableValue value) {
        if (event_sink_) {
            const auto event = flutter::EncodableValue(flutter::EncodableMap{
                {flutter::EncodableValue(kEventType), flutter::EncodableValue(kEventAsyncChannelMessage)},
                {flutter::EncodableValue(kEventValue), value},
            });
            event_sink_->Success(event);
        }
    }

    // Include the default reference counting implementation.
    IMPLEMENT_REFCOUNTING(WebviewHandler);
};

#endif  // COMMON_BROWSER_WEBVIEW_HANDLER_H_
