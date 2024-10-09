// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "webview_handler.h"

#include <sstream>
#include <string>
#include <iostream>
#include <optional>
#include <flutter/method_channel.h>
#include <flutter/method_result_functions.h>
#include <flutter/standard_method_codec.h>
#include <flutter/event_stream_handler_functions.h>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"

#include "texture_handler.h"
#include "util.h"
#include "message.h"

namespace {

// The only browser that currently get focused
CefRefPtr<CefBrowser> current_focused_browser_ = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
    CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
        .ToString();
}

const std::optional<std::pair<int, int>> GetPointFromArgs(
    const flutter::EncodableValue* args) {
    const flutter::EncodableList* list =
        std::get_if<flutter::EncodableList>(args);
    if (!list || list->size() != 2) {
        return std::nullopt;
    }
    const auto x = std::get_if<int>(&(*list)[0]);
    const auto y = std::get_if<int>(&(*list)[1]);
    if (!x || !y) {
        return std::nullopt;
    }
    return std::make_pair(*x, *y);
}

const std::optional<std::tuple<double, double, double, double, double>> GetPointAnDPIFromArgs(
    const flutter::EncodableValue* args) {
    const flutter::EncodableList* list = std::get_if<flutter::EncodableList>(args);
    if (!list || list->size() != 5) {
        return std::nullopt;
    }
    const auto dpi = std::get_if<double>(&(*list)[0]);
    const auto w = std::get_if<double>(&(*list)[1]);
    const auto h = std::get_if<double>(&(*list)[2]);
    const auto x = std::get_if<double>(&(*list)[3]);
    const auto y = std::get_if<double>(&(*list)[4]);
    if (!dpi || !w || !h || !x || !y) {
        return std::nullopt;
    }
    return std::make_tuple(*dpi, *w, *h, *x, *y);
}

int LogicalToDevice(int value, float device_scale_factor) {
    float scaled_val = static_cast<float>(value) * device_scale_factor;
    return static_cast<int>(std::floor(scaled_val));
}

CefRect LogicalToDevice(const CefRect& value, float device_scale_factor, int offsetX, int offsetY) {
    return CefRect(LogicalToDevice(value.x + offsetX, device_scale_factor),
                   LogicalToDevice(value.y + offsetY, device_scale_factor),
                   LogicalToDevice(value.width, device_scale_factor),
                   LogicalToDevice(value.height, device_scale_factor));
}

class MessageHandler : public CefMessageRouterBrowserSide::Handler {
public:
    typedef std::function<void (const CefString& request)> OnQueryCallback;

	explicit MessageHandler(OnQueryCallback cb) : onQueryCallback_(cb) {};

	bool OnQuery(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 int64_t query_id,
				 const CefString& request,
				 bool persistent,
				 CefRefPtr<Callback> callback) override {

        if (this->onQueryCallback_) this->onQueryCallback_(request);
		callback->Success("");
		return true;
	}

private:
    OnQueryCallback onQueryCallback_;

	DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

class CustomPdfPrintCallback : public CefPdfPrintCallback {
    private:
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> _result;

    public:
    CustomPdfPrintCallback(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) : _result(std::move(result)) {}

    void OnPdfPrintFinished(const CefString& path, bool ok) {
        _result->Success(ok);
    }

    IMPLEMENT_REFCOUNTING(CustomPdfPrintCallback);
};

}

const CefRefPtr<CefBrowser> WebviewHandler::CurrentFocusedBrowser() {
    return current_focused_browser_;
}

WebviewHandler::WebviewHandler(flutter::BinaryMessenger* messenger, int browser_id, float dpi) {
    const auto browser_id_str = std::to_string(browser_id);
    const auto method_channel_name = "webview_cef/" + browser_id_str;
    dpi_ = dpi;
    browser_channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
        messenger,
        method_channel_name,
        &flutter::StandardMethodCodec::GetInstance()
    );
    browser_channel_->SetMethodCallHandler([this](const auto& call, auto result) {
        HandleMethodCall(call, std::move(result));
    });

    const auto event_channel_name = "webview_cef/" + browser_id_str + "/events";
    event_channel_ = std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
        messenger,
        event_channel_name,
        &flutter::StandardMethodCodec::GetInstance()
    );

    auto handler = std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
        [this](
            const flutter::EncodableValue* arguments,
            std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&&
            events
        ) {
            event_sink_ = std::move(events);
            return nullptr;
        },
        [this](const flutter::EncodableValue* arguments) {
            return nullptr;
        }
    );

    event_channel_->SetStreamHandler(std::move(handler));
}

WebviewHandler::~WebviewHandler() {}

void WebviewHandler::Focus() {
    if (this->is_focused_) return;
    this->is_focused_ = true;
    this->browser_->GetHost()->SetFocus(true);
    current_focused_browser_ = this->browser_;
}

void WebviewHandler::Unfocus() {
    if (!this->is_focused_) return;
    this->is_focused_ = false;
    this->browser_->GetHost()->SetFocus(false);
    if (current_focused_browser_ && current_focused_browser_->IsSame(this->browser_)) {
        current_focused_browser_ = nullptr;
    }
}

void WebviewHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    if (browser->IsPopup()) return;
    EmitEvent(kEventTitleChanged, title.ToString());
}

void WebviewHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
    if (browser->IsPopup()) return;
    if (frame->IsMain()) {
        EmitEvent(kEventURLChanged, url.ToString());
    }
}

bool WebviewHandler::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                CefCursorHandle cursor,
                                cef_cursor_type_t type,
                                const CefCursorInfo& custom_cursor_info) {
    if (browser->IsPopup()) return false;

    EmitEvent(kEventCursorChanged, static_cast<int32_t>(type));
    return false;
}

void WebviewHandler::OnLoadingProgressChange(CefRefPtr<CefBrowser> browser,
                                            double progress) {
    if (browser->IsPopup()) return;
    EmitEvent(kEventLoadingProgressChanged, progress);
}

void WebviewHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                          bool isLoading,
                                          bool canGoBack,
                                          bool canGoForward) {
    if (browser->IsPopup()) return;
    EmitEvent(kEventLoadingStateChanged, isLoading);
}

void WebviewHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            TransitionType transition_type) {
    if (browser->IsPopup()) return;
    if (frame->IsMain()) {
        EmitEvent(kEventLoadStart, frame->GetURL().ToString());
    }
}

void WebviewHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        int httpStatusCode) {
    if (browser->IsPopup()) return;
    if (frame->IsMain()) {
        EmitEvent(kEventLoadEnd, static_cast<int32_t>(httpStatusCode));
    }
}

void WebviewHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (browser->IsPopup()) return;

    this->browser_ = browser;
    this->browser_channel_->InvokeMethod("onBrowserCreated", nullptr);

    // Create the browser-side router for query handling.
    CefMessageRouterConfig config;
    this->message_router_ = CefMessageRouterBrowserSide::Create(config);

    // Register handlers with the router.
    this->message_handler_.reset(new MessageHandler([this](const CefString& request) {
        auto args = std::make_unique<flutter::EncodableValue>(flutter::EncodableValue(request.ToString()));
        this->browser_channel_->InvokeMethod("onCefQuery", std::move(args));
    }));
    this->message_router_->AddHandler(message_handler_.get(), false);
}

// Returns texture_id
int64_t WebviewHandler::AttachView() {
    if (!this->onPaintCallback) {
        this->texture_handler.reset(new TextureHandler());
        this->onPaintCallback = [this](const void* buffer, int32_t width, int32_t height) {
            this->texture_handler->onPaintCallback(buffer, width, height);
        };
    }
    return this->texture_handler->texture_id();
}

void WebviewHandler::DeattachView() {
    this->onPaintCallback = nullptr;
    this->texture_handler.reset();
}

void WebviewHandler::Invalidate() {
    this->browser_->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
}

bool WebviewHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (browser->IsPopup()) {
        return false;
    }

    this->browser_channel_->SetMethodCallHandler(nullptr);
    this->browser_channel_ = nullptr;
    this->browser_ = nullptr;

    this->message_router_->RemoveHandler(message_handler_.get());
    this->message_handler_.reset();
    this->message_router_ = nullptr;
    this->texture_handler.reset();

    if (this->onBrowserClose) this->onBrowserClose();
    return false;
}

void WebviewHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    // CEF_REQUIRE_UI_THREAD();
}

// bool WebviewHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
//                                   CefRefPtr<CefFrame> frame,
//                                   const CefString& target_url,
//                                   const CefString& target_frame_name,
//                                   CefLifeSpanHandler::WindowOpenDisposition target_disposition,
//                                   bool user_gesture,
//                                   const CefPopupFeatures& popupFeatures,
//                                   CefWindowInfo& windowInfo,
//                                   CefRefPtr<CefClient>& client,
//                                   CefBrowserSettings& settings,
//                                   CefRefPtr<CefDictionaryValue>& extra_info,
//                                   bool* no_javascript_access) {
//     return true;
// }

void WebviewHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();

    // Allow Chrome to show the error page.
    if (IsChromeRuntimeEnabled())
        return;
    
    // Don't display an error for downloaded files.
    if (errorCode == ERR_ABORTED)
        return;

    if (frame->IsMain()) {
        EmitEvent(kEventLoadError, flutter::EncodableMap{
            {flutter::EncodableValue("errorCode"), flutter::EncodableValue(static_cast<int32_t>(errorCode))},
            {flutter::EncodableValue("errorText"), flutter::EncodableValue(errorText.ToString())},
            {flutter::EncodableValue("failedUrl"), flutter::EncodableValue(failedUrl.ToString())},
        });
    }
}

void WebviewHandler::CloseAllBrowsers(bool force_close) {
    if (!CefCurrentlyOn(TID_UI)) {
        // Execute on the UI thread.
        //    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
        //                                       force_close));
        return;
    }

    this->browser_->GetHost()->CloseBrowser(force_close);
}

// static
bool WebviewHandler::IsChromeRuntimeEnabled() {
    static int value = -1;
    if (value == -1) {
        CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::GetGlobalCommandLine();
        value = command_line->HasSwitch("enable-chrome-runtime") ? 1 : 0;
    }
    return value == 1;
}

void WebviewHandler::sendScrollEvent(int x, int y, int deltaX, int deltaY) {
    CefMouseEvent ev;
    ev.x = x;
    ev.y = y;
    this->browser_->GetHost()->SendMouseWheelEvent(ev, deltaX, deltaY);
}

void WebviewHandler::changeSize(float a_dpi, int w, int h)
{
    if (this->dpi_ != a_dpi) {
        this->dpi_ = a_dpi;
        this->browser_->GetHost()->NotifyScreenInfoChanged();
    }

    if (this->width_ != (uint32_t)w || this->height_ != (uint32_t)h) {
        this->width_ = w;
        this->height_ = h;
        this->browser_->GetHost()->WasResized();
    }
}

void WebviewHandler::updateViewOffset(int x, int y)
{
    this->x_ = x;
    this->y_ = y;
}

void WebviewHandler::cursorClick(int x, int y, bool up)
{
    CefMouseEvent ev;
    ev.x = x;
    ev.y = y;
    ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
    if(up && is_dragging_) {
        this->browser_->GetHost()->DragTargetDrop(ev);
        this->browser_->GetHost()->DragSourceSystemDragEnded();
        is_dragging_ = false;
    } else {
        this->browser_->GetHost()->SendMouseClickEvent(ev, CefBrowserHost::MouseButtonType::MBT_LEFT, up, 1);
    }
}

void WebviewHandler::cursorMove(int x , int y, bool dragging)
{
    CefMouseEvent ev;
    ev.x = x;
    ev.y = y;
    if(dragging) {
        ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
    }
    if(is_dragging_ && dragging) {
        this->browser_->GetHost()->DragTargetDragOver(ev, DRAG_OPERATION_EVERY);
    } else {
        this->browser_->GetHost()->SendMouseMoveEvent(ev, false);
    }
}

bool WebviewHandler::StartDragging(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefDragData> drag_data,
                                  DragOperationsMask allowed_ops,
                                  int x,
                                  int y){
    CefMouseEvent ev;
    ev.x = x;
    ev.y = y;
    ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
    this->browser_->GetHost()->DragTargetDragEnter(drag_data, ev, DRAG_OPERATION_EVERY);
    is_dragging_ = true;
    return true;
}

void WebviewHandler::OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser,
                                        double x,
                                        double y) {
    EmitEvent(kEventScrollOffsetChanged, flutter::EncodableMap{
        {flutter::EncodableValue("x"), flutter::EncodableValue(x)},
        {flutter::EncodableValue("y"), flutter::EncodableValue(y)},
    });
}

void WebviewHandler::OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                                  const CefRange& selection_range,
                                  const CefRenderHandler::RectList& character_bounds) {
    CEF_REQUIRE_UI_THREAD();

    if (!character_bounds.empty()) {
        auto firstCharacter = character_bounds.front();
        if (firstCharacter != _prevIMEPosition) {
            _prevIMEPosition = firstCharacter;
            EmitEvent(kEventIMEComposionPositionChanged, flutter::EncodableMap{
                {flutter::EncodableValue("x"), flutter::EncodableValue(static_cast<int32_t>(firstCharacter.x))},
                {flutter::EncodableValue("y"), flutter::EncodableValue(static_cast<int32_t>(firstCharacter.y + firstCharacter.height))},
            });
        }
    }
}

void WebviewHandler::sendKeyEvent(CefKeyEvent ev)
{
    this->browser_->GetHost()->SendKeyEvent(ev);
}

void WebviewHandler::loadUrl(std::string url)
{
    this->browser_->GetMainFrame()->LoadURL(url);
}

std::string WebviewHandler::getUrl()
{
    return this->browser_->GetMainFrame()->GetURL();
}

bool WebviewHandler::canGoForward() {
    return this->browser_->GetMainFrame()->GetBrowser()->CanGoForward();
}

void WebviewHandler::goForward() {
    this->browser_->GetMainFrame()->GetBrowser()->GoForward();
}

bool WebviewHandler::canGoBack() {
    return this->browser_->GetMainFrame()->GetBrowser()->CanGoBack();
}

void WebviewHandler::goBack() {
    this->browser_->GetMainFrame()->GetBrowser()->GoBack();
}

void WebviewHandler::reload() {
    this->browser_->GetMainFrame()->GetBrowser()->Reload();
}

void WebviewHandler::stopLoad() {
    this->browser_->GetMainFrame()->GetBrowser()->StopLoad();
}

void WebviewHandler::openDevTools() {
    CefWindowInfo windowInfo;
#ifdef _WIN32
    windowInfo.SetAsPopup(nullptr, "DevTools");
#endif
    this->browser_->GetHost()->ShowDevTools(windowInfo, this, CefBrowserSettings(), CefPoint());
}

void WebviewHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
    CEF_REQUIRE_UI_THREAD();

    rect.x = rect.y = 0;
    if (width_ < 1) {
        rect.width = 1;
    } else {
        rect.width = width_;
    }

    if (height_ < 1) {
        rect.height = 1;
    } else {
        rect.height = height_;
    }
}

void WebviewHandler::PrintToPDF(std::string path, const CefPdfPrintSettings& settings, std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    CefRefPtr<CustomPdfPrintCallback> callback = new CustomPdfPrintCallback(std::move(result));
    this->browser_->GetHost()->PrintToPDF(path, settings, callback);
}

bool WebviewHandler::GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) {
    if (screen_info.device_scale_factor != this->dpi_) {
        screen_info.device_scale_factor = this->dpi_;
        return true;
    }

    return false;
}

void WebviewHandler::OnPaint(CefRefPtr<CefBrowser> browser, CefRenderHandler::PaintElementType type,
                            const CefRenderHandler::RectList &dirtyRects, const void *buffer, int w, int h) {
    if (this->onPaintCallback) this->onPaintCallback(buffer, w, h);
}

void WebviewHandler::HandleMethodCall(
		const flutter::MethodCall<flutter::EncodableValue>& method_call,
		std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

    if (!this->browser_) {
        result->Error("browser not ready yet");
        return;
    }

    if (method_call.method_name().compare("loadUrl") == 0) {
        if (const auto url = std::get_if<std::string>(method_call.arguments())) {
            this->loadUrl(*url);
            result->Success();
        } else {
            result->Error("url is required");
        }
    }
    else if (method_call.method_name().compare("setSize") == 0) {
        auto tuple = GetPointAnDPIFromArgs(method_call.arguments());
        if (tuple) {
            const auto [dpi, width, height, x, y] = tuple.value();
            this->changeSize(
                static_cast<float>(dpi),
                static_cast<int>(width),
                static_cast<int>(height)
            );
            this->updateViewOffset(static_cast<int>(x), static_cast<int>(y));
        }

        result->Success();
    }
    else if (method_call.method_name().compare("cursorClickDown") == 0) {
        this->Focus();

        const auto point = GetPointFromArgs(method_call.arguments());
        this->cursorClick(point->first, point->second, false);
        result->Success();
    }
    else if (method_call.method_name().compare("cursorClickUp") == 0) {
        const auto point = GetPointFromArgs(method_call.arguments());
        this->cursorClick(point->first, point->second, true);
        result->Success();
    }
    else if (method_call.method_name().compare("cursorMove") == 0) {
        const auto point = GetPointFromArgs(method_call.arguments());
        this->cursorMove(point->first, point->second, false);
        result->Success();
    }
    else if (method_call.method_name().compare("cursorDragging") == 0) {
        const auto point = GetPointFromArgs(method_call.arguments());
        this->cursorMove(point->first, point->second, true);
        result->Success();
    }
    else if (method_call.method_name().compare("setScrollDelta") == 0) {
        const flutter::EncodableList* list =
            std::get_if<flutter::EncodableList>(method_call.arguments());
        const auto x = *std::get_if<int>(&(*list)[0]);
        const auto y = *std::get_if<int>(&(*list)[1]);
        const auto deltaX = *std::get_if<int>(&(*list)[2]);
        const auto deltaY = *std::get_if<int>(&(*list)[3]);
        this->sendScrollEvent(x, y, deltaX, deltaY);
        result->Success();
    }
    else if (method_call.method_name().compare("setZoomLevel") == 0) {
        const auto level = std::get_if<double>(method_call.arguments());
        if (level) browser_->GetHost()->SetZoomLevel(*level);
        result->Success();
    }
    else if (method_call.method_name().compare("getZoomLevel") == 0) {
        result->Success(browser_->GetHost()->GetZoomLevel());
    }
    else if (method_call.method_name().compare("unfocus") == 0) {
        this->Unfocus();
        result->Success();
    }
    else if (method_call.method_name().compare("focus") == 0) {
        this->Focus();
        result->Success();
    }
    else if (method_call.method_name().compare("goForward") == 0) {
        this->goForward();
        result->Success();
    }
    else if (method_call.method_name().compare("canGoForward") == 0) {
        result->Success(flutter::EncodableValue(this->canGoForward()));
    }
    else if (method_call.method_name().compare("goBack") == 0) {
        this->goBack();
        result->Success();
    }
    else if (method_call.method_name().compare("canGoBack") == 0) {
        result->Success(flutter::EncodableValue(this->canGoBack()));
    }
    else if (method_call.method_name().compare("stopLoad") == 0) {
        this->stopLoad();
        result->Success();
    }
    else if (method_call.method_name().compare("reload") == 0) {
        this->reload();
        result->Success();
    }
    else if (method_call.method_name().compare("openDevTools") == 0) {
        this->openDevTools();
        result->Success();
    }
    else if (method_call.method_name().compare("evaluateJavaScript") == 0) {
        auto msg = async_channel_message::EvaluateJavaScript::CreateCefProcessMessage(method_call.arguments());
        if (!msg) {
            result->Error(kErrorInvalidArguments);
            return;
        }

        this->browser_->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
        result->Success();
    }
    else if (method_call.method_name().compare("printToPDF") == 0) {
        const flutter::EncodableMap* m = std::get_if<flutter::EncodableMap>(method_call.arguments());

        auto filepath = util::GetStringFromMap(m, "path");
        if (!filepath) {
            result->Error("path is required");
            return;
        }

        CefPdfPrintSettings printSettings = CefPdfPrintSettings{};

        auto backgrounds_enabled = util::GetBoolFromMap(m, "backgroundsEnabled");
        if (backgrounds_enabled) {
            printSettings.print_background = true;
        }

        auto page_width = util::GetIntFromMap(m, "pageWidth");
        auto page_height = util::GetIntFromMap(m, "pageHeight");
        if (page_width && page_height) {
            printSettings.paper_width = *page_width;
            printSettings.paper_height = *page_height;
        }

        this->PrintToPDF(*filepath, printSettings, std::move(result));
    }
    else if (method_call.method_name().compare("attachView") == 0) {
        result->Success(flutter::EncodableValue(this->AttachView()));
    }
    else if (method_call.method_name().compare("deattachView") == 0) {
        this->DeattachView();
        result->Success();
    }
    else if (method_call.method_name().compare("invalidate") == 0) {
        this->Invalidate();
        result->Success();
    }
    else if (method_call.method_name().compare("dispose") == 0) {
        this->Unfocus();

        this->browser_->GetHost()->CloseBrowser(false);
        result->Success();
    }
    else {
        result->NotImplemented();
    }
}

bool WebviewHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                              CefRefPtr<CefFrame> frame,
                                              CefProcessId source_process,
                                              CefRefPtr<CefProcessMessage> message) {

    auto message_name = message->GetName();
    if (message_name == ipc::EvaluateJavaScriptResponse) {
        auto v = async_channel_message::EvaluateJavaScript::CreateFlutterChannelMessage(message);
        this->EmitAsyncChannelMessage(v);
        return true;
    }

    return this->message_router_->OnProcessMessageReceived(browser, frame, source_process, message);
}

void WebviewHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                               TerminationStatus status,
                                               int error_code,
                                               const CefString& error_string) {
    CEF_REQUIRE_UI_THREAD();

    this->message_router_->OnRenderProcessTerminated(browser);
}

bool WebviewHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    bool user_gesture,
                                    bool is_redirect) {
    CEF_REQUIRE_UI_THREAD();

    this->message_router_->OnBeforeBrowse(browser, frame);
    return false;
}