part of webview;

typedef TitleChangeCallback = void Function(String title);
typedef UrlChangeCallback = void Function(String url);
typedef CefQueryCallback = void Function({String? request});
typedef ScrollOffsetChangedCallback = void Function(double x, double y);
typedef LoadingProgressChangedCallback = void Function(double v);
typedef LoadingStateChangedCallback = void Function(bool isLoading);
typedef LoadStartCallback = void Function(String url);
typedef LoadEndCallback = void Function(int statusCode);
typedef LoadErrorCallback = void Function(int code, String text, String url);

const MethodChannel _pluginChannel = MethodChannel("webview_cef");
bool _hasCallStartCEF = false;
final _cefStarted = Completer();

_startCEF() async {
  if (!_hasCallStartCEF) {
    _hasCallStartCEF = true;

    _pluginChannel.setMethodCallHandler((call) async {
      if (call.method == 'onCEFInitialized') {
        _cefStarted.complete();
      }
    });

    _pluginChannel.invokeMethod('startCEF', {
      'cachePath': GlobalCefSettings.cachePath,
      'rootCachePath': GlobalCefSettings.rootCachePath,
    });
  }

  await _cefStarted.future;
}

const _kEventTitleChanged = "titleChanged";
const _kEventURLChanged = "urlChanged";
const _kEventCursorChanged = "cursorChanged";
const _kEventScrollOffsetChanged = "scrollOffsetChanged";
const _kEventLoadingProgressChanged = "loadingProgressChanged";
const _kEventLoadingStateChanged = "loadingStateChanged";
const _kEventLoadStart = "loadStart";
const _kEventLoadEnd = "loadEnd";
const _kEventLoadError = "loadError";
const _kIMEComposionPositionChanged = "imeComposionPositionChanged";
const _kEventAsyncChannelMessage = 'asyncChannelMessage';

class WebViewController extends ChangeNotifier {
  static int _id = 0;

  final Completer<void> _creatingCompleter = Completer<void>();
  bool _isInitialized = false;

  bool _headless = false;
  bool get isHeadless => _headless;

  late final int _browserID;
  int get browserID => _browserID;

  Completer<int> _textureIdCompleter = Completer();
  bool _isDisposed = false;
  late final MethodChannel _broswerChannel;
  late final EventChannel _eventChannel;
  StreamSubscription? _eventStreamSubscription;

  final ValueNotifier<CursorType> _cursorType = ValueNotifier(CursorType.pointer);

  Future<void> get ready => _creatingCompleter.future;

  TitleChangeCallback? onTitleChanged;
  UrlChangeCallback? onUrlChanged;
  ScrollOffsetChangedCallback? onScrollOffsetChanged;

  /// Called when the overall page loading progress has changed.
  /// progress ranges from 0.0 to 1.0.
  LoadingProgressChangedCallback? onLoadingProgressChanged;

  /// Called when the loading state has changed. This callback will be executed
  /// twice -- once when loading is initiated either programmatically or by user
  /// action, and once when loading is terminated due to completion, cancellation
  /// of failure. It will be called before any calls to OnLoadStart and after all
  /// calls to OnLoadError and/or OnLoadEnd.
  LoadingStateChangedCallback? onLoadingStateChanged;

  LoadStartCallback? onLoadStart;
  LoadEndCallback? onLoadEnd;
  LoadErrorCallback? onLoadError;
  CefQueryCallback? onCefQuery;

  WebViewController({
    bool headless = false,
  }) : _headless = headless;

  /// Initializes the underlying platform view.
  Future<void> initialize() async {
    assert(!_isDisposed);

    if (_isDisposed || _isInitialized) return;
    _isInitialized = true;

    await _startCEF();

    try {
      _browserID = ++_id;
      _broswerChannel = MethodChannel('webview_cef/$_browserID');
      _broswerChannel.setMethodCallHandler(_methodCallhandler);

      final createBrowserArgs = {
        'browserID': _browserID,
        'headless': _headless,
        'dpi': PlatformDispatcher.instance.implicitView?.devicePixelRatio,
      };
      final textureId = await _pluginChannel.invokeMethod<int>('createBrowser', createBrowserArgs) ?? 0;
      if (textureId != 0) _textureIdCompleter.complete(textureId);
      _eventChannel = EventChannel('webview_cef/$_browserID/events');
      _eventStreamSubscription = _eventChannel.receiveBroadcastStream().listen(_handleBrowserEvents);
    } on PlatformException catch (e) {
      _creatingCompleter.completeError(e);
    }
    notifyListeners();

    return _creatingCompleter.future;
  }

  deattachView() async {
    assert(!_headless);

    _headless = true;
    _textureIdCompleter = Completer();
    await _broswerChannel.invokeMethod<int>('deattachView');
    notifyListeners();
  }

  attachView() async {
    assert(_headless);

    _headless = false;
    _broswerChannel.invokeMethod<int>('attachView').then((tid) {
      _textureIdCompleter.complete(tid);
    });
    notifyListeners();
  }

  invalidate() async {
    await _broswerChannel.invokeMethod<int>('invalidate');
  }

  Future<dynamic> _methodCallhandler(MethodCall call) async {
    switch (call.method) {
      case 'onBrowserCreated':
        _creatingCompleter.complete();
        return null;
      case 'onCefQuery':
        onCefQuery?.call(request: call.arguments);
        return null;
    }

    return null;
  }

  _handleBrowserEvents(dynamic event) {
    final m = event as Map<dynamic, dynamic>;
    switch (m['type']) {
      case _kEventURLChanged:
        onUrlChanged?.call(m['value'] as String);
        return;
      case _kEventTitleChanged:
        onTitleChanged?.call(m['value'] as String);
        return;
      case _kEventCursorChanged:
        _cursorType.value = CursorType.values[m['value'] as int];
        return;
      case _kEventScrollOffsetChanged:
        final offset = m['value'] as Map<dynamic, dynamic>;
        onScrollOffsetChanged?.call(offset['x'] as double, offset['y'] as double);
        return;
      case _kEventLoadingProgressChanged:
        onLoadingProgressChanged?.call(m['value'] as double);
        return;
      case _kEventLoadingStateChanged:
        onLoadingStateChanged?.call(m['value'] as bool);
        return;
      case _kEventLoadStart:
        onLoadStart?.call(m['value'] as String);
        return;
      case _kEventLoadEnd:
        onLoadEnd?.call(m['value'] as int);
        return;
      case _kEventLoadError:
        final data = m['value'] as Map<dynamic, dynamic>;
        onLoadError?.call(
          data['errorCode'] as int,
          data['errorText'] as String,
          data['failedUrl'] as String,
        );
        return;
      case _kIMEComposionPositionChanged:
        final pos = m['value'] as Map<dynamic, dynamic>;
        _onIMEComposionPositionChanged?.call((pos['x'] as int).toDouble(), (pos['y'] as int).toDouble());
        return;
      case _kEventAsyncChannelMessage:
        _AsyncChannelMessageManager.handleChannelEvents(m['value']);
        return;
      default:
    }
  }

  Function(double, double)? _onIMEComposionPositionChanged;

  @override
  Future<void> dispose() async {
    await _creatingCompleter.future;
    if (!_isDisposed) {
      _isDisposed = true;
      await _broswerChannel.invokeMethod('dispose');
      _eventStreamSubscription?.cancel();
      _cursorType.dispose();
    }
    super.dispose();
  }

  /// Loads the given [url].
  Future<void> loadUrl(String url) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('loadUrl', url);
  }

  Future<String?> getUrl() async {
    assert(!_isDisposed);
    if (_isDisposed) return null;

    return _broswerChannel.invokeMethod('getUrl');
  }

  Future<void> stopLoad() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('stopLoad');
  }

  /// Reloads the current document.
  Future<void> reload() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('reload');
  }

  Future<bool> canGoForward() async {
    assert(!_isDisposed);
    if (_isDisposed) return false;

    return await _broswerChannel.invokeMethod('canGoForward');
  }

  Future<void> goForward() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('goForward');
  }

  Future<bool> canGoBack() async {
    assert(!_isDisposed);
    if (_isDisposed) return false;

    return await _broswerChannel.invokeMethod('canGoBack');
  }

  Future<void> goBack() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('goBack');
  }

  Future<dynamic> evaluateJavaScript(String code, [bool throwEvalError = false]) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    final message = EvaluateJavaScriptMessage(code, throwEvalError: throwEvalError);
    return _AsyncChannelMessageManager.invokeMethod(_broswerChannel, message);
  }

  /// If true, allows "Ctrl + +/-" and "Ctrl + mouse wheel" to control page scaling
  bool allowShortcutZoom = false;
  double _zoomLevel = 1.0;
  Future<double?> getZoomLevel() async {
    assert(!_isDisposed);
    if (_isDisposed) return null;

    final v = await _broswerChannel.invokeMethod<double>('getZoomLevel');
    if (v != null) _zoomLevel = v;
    return v;
  }

  Future<void> setZoomLevel(double level) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    await _broswerChannel.invokeMethod('setZoomLevel', level);
    _zoomLevel = level;
  }

  Future<void> _increaseZoomLevel(double dz) => setZoomLevel(_zoomLevel + dz);

  Future<void> openDevTools() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('openDevTools');
  }

  /// Prints current page as PDF file.
  /// If successes, returns true and file will be saved at [filepath].
  /// The output paper size can be specified by [pageWidth] and [pageHeight] in
  /// microns, if either of these values is less than or equal to zero then the
  /// default paper size (A4) will be used.
  /// Set [backgroundsEnabled] to true to print background graphics or false to
  /// not print background graphics.
  Future<bool> printToPDF({
    required String filepath,
    int? pageWidth,
    int? pageHeight,
    bool backgroundsEnabled = true,
  }) async {
   assert(!_isDisposed);
    if (_isDisposed) return false;

    return (await _broswerChannel.invokeMethod<bool>('printToPDF', {
      'path': filepath,
      'pageWidth': pageWidth,
      'pageHeight': pageHeight,
      'backgroundsEnabled': backgroundsEnabled,
    })) ?? false;
  }

  Future<void> focus() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('focus');
  }

  Future<void> _unfocus() async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod('unfocus');
  }

  /// Moves the virtual cursor to [position].
  Future<void> _cursorMove(Offset position) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel
        .invokeMethod('cursorMove', [position.dx.round(), position.dy.round()]);
  }

  Future<void> _cursorDragging(Offset position) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod(
        'cursorDragging', [position.dx.round(), position.dy.round()]);
  }

  Future<void> _cursorClickDown(Offset position) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod(
        'cursorClickDown', [position.dx.round(), position.dy.round()]);
  }

  Future<void> _cursorClickUp(Offset position) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod(
        'cursorClickUp', [position.dx.round(), position.dy.round()]);
  }

  /// Sets the horizontal and vertical scroll delta.
  Future<void> _setScrollDelta(Offset position, int dx, int dy) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel.invokeMethod(
        'setScrollDelta', [position.dx.round(), position.dy.round(), dx, dy]);
  }

  /// Sets the surface size to the provided [size].
  Future<void> _setSize(double dpi, Size size, Offset viewOffset) async {
    assert(!_isDisposed);
    if (_isDisposed) return;

    return _broswerChannel
        .invokeMethod('setSize', [dpi, size.width, size.height, viewOffset.dx, viewOffset.dy]);
  }
}
