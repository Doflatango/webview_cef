part of webview;

mixin _WebViewTextInput implements DeltaTextInputClient {
  String _composingText = '';

  @override
  TextEditingValue? currentTextEditingValue;

  @override
  AutofillScope? currentAutofillScope;

  TextInputConnection? _textInputConnection;

  attachTextInputClient() {
    _textInputConnection?.close();
    _textInputConnection = TextInput.attach(this, const TextInputConfiguration(enableDeltaModel: true));
  }

  detachTextInputClient() {
    _textInputConnection?.close();
  }

  updateIMEComposionPosition(double x, double y, Offset offset) {
    /// It always displays at the last position, which should be a bug in the Flutter engine.
    _textInputConnection?.setEditableSizeAndTransform(const Size(0, 0),
        Matrix4.translationValues(offset.dx + x, offset.dy + y, 0));
  }

  @override
  updateEditingValueWithDeltas(List<TextEditingDelta> textEditingDeltas) {
    /// Handles IME composition only
    for (var d in textEditingDeltas) {
      if (d is TextEditingDeltaInsertion) {
        // composing text
        if (d.composing.isValid) {
          _composingText += d.textInserted;
          _pluginChannel.invokeMethod('imeSetComposition', _composingText);
        }
      } else if (d is TextEditingDeltaReplacement) {
        if (d.composing.isValid) {
          // _composingText = d.replacementText;
          _pluginChannel.invokeMethod('imeCommitText', d.replacementText);
        }
      } else if (d is TextEditingDeltaNonTextUpdate) {
        if (_composingText.isNotEmpty) {
          // _pluginChannel.invokeMethod('imeCommitText', _composingText);
          _composingText = '';
        }
      }
    }
  }

  @override
  didChangeInputControl(TextInputControl? oldControl, TextInputControl? newControl) {}

  @override
  connectionClosed() {}

  @override
  insertTextPlaceholder(Size size) {}

  @override
  performAction(TextInputAction action) {}

  @override
  performPrivateCommand(String action, Map<String, dynamic> data) {}

  @override
  performSelector(String selectorName) {}

  @override
  removeTextPlaceholder() {}

  @override
  showAutocorrectionPromptRect(int start, int end) {}

  @override
  showToolbar() {}

  @override
  updateEditingValue(TextEditingValue value) {}

  @override
  updateFloatingCursor(RawFloatingCursorPoint point) {}

  @override
  void insertContent(KeyboardInsertedContent content) {}
}
