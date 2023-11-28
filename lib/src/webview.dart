library webview;

import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

part 'cef_settings.dart';
part 'webview_cursor.dart';
part 'async_channel_message.dart';
part 'text_input.dart';
part 'webview_controller.dart';

class WebView extends StatefulWidget {
  final WebViewController controller;

  const WebView(this.controller, {Key? key}) : super(key: key);

  @override
  WebViewState createState() => WebViewState();
}

class WebViewState extends State<WebView> with _WebViewTextInput {
  final GlobalKey _key = GlobalKey();
  final _focusNode = FocusNode();

  WebViewController get _controller => widget.controller;

  @override
  void initState() {
    super.initState();

    _controller._onIMEComposionPositionChanged = (x, y) {
      final box = _key.currentContext!.findRenderObject() as RenderBox;
      updateIMEComposionPosition(x, y, box.localToGlobal(Offset.zero));
    };

    /// Update the widget once the browser being ready
    _controller.ready.then((_) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _reportSurfaceSize(context);
        setState(() {});
      });
    });
  }

  @override
  void dispose() {
    detachTextInputClient();
    _focusNode.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SizedBox.expand(key: _key, child: _buildInner());
  }

  bool _controlKeyDown = false;
  Widget _buildInner() {
    return NotificationListener<SizeChangedLayoutNotification>(
      onNotification: (notification) {
        _reportSurfaceSize(context);
        return true;
      },
      child: SizeChangedLayoutNotifier(
        child: Focus(
          debugLabel: 'webview cef',
          focusNode: _focusNode,
          autofocus: false,
          onFocusChange: (focused) {
            // print('webview onFocusChange chagned to: ${FocusScope.of(context).focusedChild?.toString()}');
            // if (!focused) _controller._unfocus();
          },
          onKeyEvent: _handleKeyEvent,
          child: Listener(
            onPointerHover: (ev) {
              _controller._cursorMove(ev.localPosition);
            },
            onPointerDown: (ev) async {
              _controller._cursorClickDown(ev.localPosition);

              // Fixes for getting focus immediately.
              if (!_focusNode.hasFocus) {
                // _focusNode.unfocus();
                // await Future<void>.delayed(const Duration(milliseconds: 1));
                if (mounted) FocusScope.of(context).requestFocus(_focusNode);
                attachTextInputClient();
              }
            },
            onPointerUp: (ev) {
              _controller._cursorClickUp(ev.localPosition);
            },
            onPointerMove: (ev) {
              _controller._cursorDragging(ev.localPosition);
            },
            onPointerSignal: (signal) {
              if (signal is PointerScrollEvent) {
                var dy = signal.scrollDelta.dy.round();
                if (_shouldUpdateZoomLevel()) {
                  _controller._increaseZoomLevel(dy > 0 ? -kZoomLevelUnit : kZoomLevelUnit);
                  return;
                }

                if (!Platform.isMacOS) dy = -dy;
                _controller._setScrollDelta(signal.localPosition,
                    signal.scrollDelta.dx.round(), dy);
              }
            },
            onPointerPanZoomUpdate: (event) {
              _controller._setScrollDelta(event.localPosition,
                  event.panDelta.dx.round(), event.panDelta.dy.round());
            },
            child: ValueListenableBuilder<CursorType>(
              valueListenable: _controller._cursorType,
              child: Texture(textureId: _controller._textureId),
              builder: (context, value, child) {
                return MouseRegion(
                  cursor: value.transform,
                  child: child,
                  onExit: (event) => _controller._unfocus(),
                );
              },
            ),
          ),
        ),
      ),
    );
  }

  KeyEventResult _handleKeyEvent(FocusNode node, KeyEvent event) {
    if (event.logicalKey == LogicalKeyboardKey.controlLeft || event.logicalKey == LogicalKeyboardKey.controlRight) {
      if (event is KeyDownEvent) {
        _controlKeyDown = true;
      } else if (event is KeyUpEvent) {
        _controlKeyDown = false;
      }
      return KeyEventResult.ignored;
    } else if (event.logicalKey == LogicalKeyboardKey.equal) {
      if (_shouldUpdateZoomLevel()) {
        _controller._increaseZoomLevel(kZoomLevelUnit);
        return KeyEventResult.handled;
      }
    } else if (event.logicalKey == LogicalKeyboardKey.minus) {
      if (_shouldUpdateZoomLevel()) {
        _controller._increaseZoomLevel(-kZoomLevelUnit);
        return KeyEventResult.handled;
      }
    }

    return KeyEventResult.ignored;
  }

  static const kZoomLevelUnit = 0.25;
  bool _shouldUpdateZoomLevel() => _controller.allowShortcutZoom && _controlKeyDown;

  void _reportSurfaceSize(BuildContext context) async {
    final box = _key.currentContext?.findRenderObject() as RenderBox?;
    if (box != null) {
      final dpi = MediaQuery.of(context).devicePixelRatio;
      await _controller.ready;
      final translation = box.getTransformTo(null).getTranslation();
      unawaited(_controller._setSize(
        dpi,
        Size(box.size.width, box.size.height),
        Offset(translation.x, translation.y),
        // box.localToGlobal(Offset.zero),
      ));
    }
  }
}
