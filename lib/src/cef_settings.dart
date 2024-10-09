part of webview;

/// Global [CefSettings], will applies to all webview instance.
/// Update this before you are initialing any [WebViewController]s.
final CefSettings GlobalCefSettings = CefSettings();

class CefSettings {
  /// The location where data for the global browser cache will be stored on
  /// disk. If this value is non-empty then it must be an absolute path that is
  /// either equal to or a child directory of [CefSettings.rootCachePath]. If
  /// this value is empty then browsers will be created in "incognito mode" where
  /// in-memory caches are used for storage and no data is persisted to disk.
  /// HTML5 databases such as localStorage will only persist across sessions if a
  /// cache path is specified. Can be overridden for individual CefRequestContext
  /// instances via the CefRequestContextSettings.cache_path value. When using
  /// the Chrome runtime the "default" profile will be used if [cachePath] and
  /// [rootCachePath] have the same value.
  String? cachePath;

  /// The root directory that all [CefSettings.cachePath] and
  /// CefRequestContextSettings.cache_path values must have in common. If this
  /// value is empty and [CefSettings.cachePath] is non-empty then it will
  /// default to the [CefSettings.cachePath] value. If this value is non-empty
  /// then it must be an absolute path. Failure to set this value correctly may
  /// result in the sandbox blocking read/write access to the [cachePath]
  /// directory.
  String? rootCachePath;
}