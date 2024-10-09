#include "webview_cef_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <thread>

#include "browser/webview_app.h"
#include "texture_handler.h"

#define ColorUNDERLINE \
  0xFF000000  // Black SkColor value for underline,
              // same as Blink.
#define ColorBKCOLOR \
  0x00000000  // White SkColor value for background,
              // same as Blink.

namespace webview_cef {
	bool init = false;

	bool composingText = false;

	flutter::BinaryMessenger* messenger;

	CefRefPtr<WebviewApp> app;
	CefMainArgs mainArgs;

	void startCEF(CefSettings cefs) {
		CefWindowInfo window_info;
		CefBrowserSettings settings;
		window_info.SetAsWindowless(nullptr);

		cefs.windowless_rendering_enabled = true;
		CefInitialize(mainArgs, cefs, app.get(), nullptr);
		CefRunMessageLoop();
		CefShutdown();
	}

	template <typename T>
	std::optional<T> GetOptionalValue(const flutter::EncodableMap& map, const char* key) {
		const auto it = map.find(flutter::EncodableValue(key));
		if (it != map.end()) {
			const auto val = std::get_if<T>(&(it->second));
			if (val) {
				return *val;
			}
		}
		return std::nullopt;
	}

	CefSettings GetCefSettings(const flutter::MethodCall<flutter::EncodableValue>& method_call) {
		const flutter::EncodableMap* map = std::get_if<flutter::EncodableMap>(method_call.arguments());
		CefSettings settings;
		if (!map) return settings;

		const auto cache_path = GetOptionalValue<std::string>(*map, "cachePath");
		if (cache_path) CefString(&settings.cache_path) = cache_path.value();

		const auto root_cache_path = GetOptionalValue<std::string>(*map, "rootCachePath");
		if (root_cache_path) CefString(&settings.root_cache_path) = root_cache_path.value();

		return settings;
	}

	// static
	void WebviewCefPlugin::RegisterWithRegistrar(
		flutter::PluginRegistrarWindows* registrar) {
		TextureHandler::InitTextureRegistrar(registrar->texture_registrar());
		messenger = registrar->messenger();
		auto plugin_channel =
			std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
				messenger, "webview_cef", &flutter::StandardMethodCodec::GetInstance());

		auto plugin = std::make_unique<WebviewCefPlugin>();
		plugin_channel->SetMethodCallHandler(
			[plugin_pointer = plugin.get()](const auto& call, auto result) {
				plugin_pointer->HandleMethodCall(call, std::move(result));
			});
		app = new WebviewApp(std::move(plugin_channel));
		registrar->AddPlugin(std::move(plugin));
	}

	void WebviewCefPlugin::sendKeyEvent(CefKeyEvent ev)
	{
		auto broswer = WebviewHandler::CurrentFocusedBrowser();
		if (broswer) {
			broswer->GetHost()->SendKeyEvent(ev);
		}
	}

	WebviewCefPlugin::WebviewCefPlugin() {}

	WebviewCefPlugin::~WebviewCefPlugin() {}

	void WebviewCefPlugin::HandleMethodCall(
		const flutter::MethodCall<flutter::EncodableValue>& method_call,
		std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
		if (method_call.method_name().compare("startCEF") == 0) {
			if (!init) {
				auto cefSettings = GetCefSettings(method_call);
				new std::thread([cefSettings](){
					startCEF(cefSettings);
				});
				init = true;
			}
			result->Success();
		} else if (method_call.method_name().compare("createBrowser") == 0) {
			const flutter::EncodableMap* map = std::get_if<flutter::EncodableMap>(method_call.arguments());
			if (!map) {
				result->Error("NoArguments");
				return;
			}

			const auto browser_id = GetOptionalValue<int>(*map, "browserID");
			if (!browser_id) {
				result->Error("InvalidArguments", "browserID");
				return;
			}

			const auto headless = GetOptionalValue<bool>(*map, "headless").value_or(false);
			const auto dpi = GetOptionalValue<double>(*map, "dpi").value_or(1);
			auto handler = new WebviewHandler(messenger, *browser_id, (float)dpi);
			app->CreateBrowser(handler);
			if (headless) {
				result->Success();
			} else {
				auto const texture_id = handler->AttachView();
				result->Success(flutter::EncodableValue(texture_id));
			}
		} else if (method_call.method_name().compare("imeSetComposition") == 0) {
			auto browser = WebviewHandler::CurrentFocusedBrowser();
			if (browser) {
				const auto text = *std::get_if<std::string>(method_call.arguments());
				CefString cTextStr = CefString(text);

				std::vector<CefCompositionUnderline> underlines;
				cef_composition_underline_t underline = {};
				underline.range.from = 0;
				underline.range.to = static_cast<int>(0 + cTextStr.length());
				underline.color = ColorUNDERLINE;
				underline.background_color = ColorBKCOLOR;
				underline.thick = 0;
				underline.style = CEF_CUS_DOT;
				underlines.push_back(underline);

				// Keeps the caret at the end of the composition
				auto selection_range_end = static_cast<int>(0 + cTextStr.length());
				CefRange selection_range = CefRange(selection_range_end, selection_range_end);
				browser->GetHost()->ImeSetComposition(cTextStr, underlines, CefRange(UINT32_MAX, UINT32_MAX), selection_range);
			}
			result->Success();
		} else if (method_call.method_name().compare("imeCommitText") == 0) {
			auto browser = WebviewHandler::CurrentFocusedBrowser();
			if (browser) {
				const auto text = *std::get_if<std::string>(method_call.arguments());
				CefString cTextStr = CefString(text);
				// Send the text to the browser. The |replacement_range| and
				// |relative_cursor_pos| params are not used on Windows, so provide
				// default invalid values.
				browser->GetHost()->ImeCommitText(cTextStr,
												  CefRange(UINT32_MAX, UINT32_MAX), 0);
			}
			result->Success();
		} else if (method_call.method_name().compare("imeFinishComposingText") == 0) {
			auto browser = WebviewHandler::CurrentFocusedBrowser();
			if (browser) {
				browser->GetHost()->ImeFinishComposingText(false);
			}
			result->Success();
		} else if (method_call.method_name().compare("imeCancelComposition") == 0) {
			auto browser = WebviewHandler::CurrentFocusedBrowser();
			if (browser) {
				browser->GetHost()->ImeCancelComposition();
			}
			result->Success();
		} else {
			result->NotImplemented();
		}
	}

}  // namespace webview_cef
