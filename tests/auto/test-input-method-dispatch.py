#!/usr/bin/env python3
"""Run the production IME dispatcher with Qt strings and a recording backend.

Requires a C++ compiler and Qt5Gui development files, but no Gecko runtime.
Extracting the method keeps this test independent of QMozView's render backend.
Pass a qmozview_p.cpp path to check a different revision.
"""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


source = (Path(sys.argv[1]) if len(sys.argv) > 1 else
          Path(__file__).resolve().parents[2] / "src/qmozview_p.cpp")
text = source.read_text()
start = text.index("void QMozViewPrivate::dispatchInputMethodEvent(")
end = text.index("\nvoid QMozViewPrivate::clearPendingInputMethodEvents()", start)
dispatcher = text[start:end]

harness = r'''
#include <QGuiApplication>
#include <QInputMethod>
#include <QString>
#include <cassert>
#include <cstdint>
#include <iostream>

struct PendingInputMethodEvent {
    QString commit, preedit;
    int replacementStart = 0, replacementLength = 0;
    qint64 replacementOffset = -1;
    bool hadPreedit = false;
};
struct QMozViewPrivate {
    bool mViewInitialized = true;
    Qt::InputMethodHints mInputMethodHints = Qt::ImhNone;
    int presses = 0, releases = 0, texts = 0, key = 0, character = 0;
    bool absolute = false;
    PendingInputMethodEvent sent;
    void dispatchInputMethodEvent(const PendingInputMethodEvent &event);
};
namespace MozKey {
int QtKeyCodeToDOMKeyCode(int key, Qt::KeyboardModifiers) { return key; }
}
namespace QtMoz {
void chromeSessionSendKeyPress(QMozViewPrivate *v, int key, int, int character) {
    ++v->presses; v->key = key; v->character = character;
}
void chromeSessionSendKeyRelease(QMozViewPrivate *v, int, int, int) {
    ++v->releases;
}
void chromeSessionSendTextEvent(QMozViewPrivate *v, const QString &commit,
                               const QString &preedit, int start, int length) {
    ++v->texts; v->sent.commit = commit; v->sent.preedit = preedit;
    v->sent.replacementStart = start; v->sent.replacementLength = length;
}
void chromeSessionSendTextEventAtOffset(QMozViewPrivate *v, const QString &commit,
                                       const QString &preedit, quint32 offset,
                                       int length) {
    chromeSessionSendTextEvent(v, commit, preedit, 0, length);
    v->absolute = true; v->sent.replacementOffset = offset;
}
}
// DISPATCHER
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    int checked = 0;
    for (auto hints : {Qt::ImhFormattedNumbersOnly,
                       Qt::ImhDialableCharactersOnly, Qt::ImhNone}) {
        for (const char *value : {"123456", "001234", "000000", "12",
                                  "2147483647", "99999999999999999999",
                                  "+33123456789", "-12.5", "hello"}) {
            for (int replacement : {0, 1, 2}) {
                QMozViewPrivate view;
                view.mInputMethodHints = hints;
                PendingInputMethodEvent event;
                event.commit = QString::fromLatin1(value);
                event.replacementStart = replacement == 1 ? -6 : 0;
                event.replacementLength = replacement ? 6 : 0;
                event.replacementOffset = replacement == 2 ? 4 : -1;
                view.dispatchInputMethodEvent(event);
                assert(view.presses == 0 && view.releases == 0);
                assert(view.texts == 1 && view.sent.commit == event.commit);
                assert(view.sent.replacementLength == event.replacementLength);
                assert(view.absolute == (replacement == 2));
                assert(view.sent.replacementStart == event.replacementStart);
                assert(view.sent.replacementOffset == event.replacementOffset);
                ++checked;
            }
        }
        for (char digit = '0'; digit <= '9'; ++digit) {
            QMozViewPrivate view;
            view.mInputMethodHints = hints;
            PendingInputMethodEvent event;
            event.commit = QChar(digit);
            view.dispatchInputMethodEvent(event);
            if (hints == Qt::ImhNone) {
                assert(view.texts == 1 && view.sent.commit == event.commit);
                assert(view.presses == 0 && view.releases == 0);
            } else {
                assert(view.presses == 1 && view.releases == 1);
                assert(view.key == digit && view.character == digit);
                assert(view.texts == 0);
            }
            ++checked;
        }
    }
    std::cout << checked << " input-method dispatch cases passed\n";
}
'''

with tempfile.TemporaryDirectory(prefix="qtmoz-input-method-") as directory:
    cpp = Path(directory) / "dispatch.cpp"
    binary = Path(directory) / "dispatch"
    cpp.write_text(harness.replace("// DISPATCHER", dispatcher))
    flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "Qt5Gui"], text=True))
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) +
                   ["-std=c++11", "-fPIC", str(cpp), "-o", str(binary)] + flags,
                   check=True)
    subprocess.run([str(binary)], check=True,
                   env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
