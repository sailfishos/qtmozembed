/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Qt>

#include "EmbedQtKeyUtils.h"

#include "nsIDOMWindowUtils.h"
#include "mozilla/ArrayUtils.h"      // for ArrayLength


struct nsKeyConverter {
    int vkCode; // Platform independent key code
    int keysym; // Qt key code
};

// Copied from KeyboardEventBinding.h (JB#50950)
namespace mozilla {
namespace dom {
namespace KeyboardEventBinding {
  static const uint32_t DOM_KEY_LOCATION_STANDARD = 0;
  static const uint32_t DOM_KEY_LOCATION_LEFT = 1;
  static const uint32_t DOM_KEY_LOCATION_RIGHT = 2;
  static const uint32_t DOM_KEY_LOCATION_NUMPAD = 3;
  static const uint32_t DOM_VK_CANCEL = 3;
  static const uint32_t DOM_VK_HELP = 6;
  static const uint32_t DOM_VK_BACK_SPACE = 8;
  static const uint32_t DOM_VK_TAB = 9;
  static const uint32_t DOM_VK_CLEAR = 12;
  static const uint32_t DOM_VK_RETURN = 13;
  static const uint32_t DOM_VK_SHIFT = 16;
  static const uint32_t DOM_VK_CONTROL = 17;
  static const uint32_t DOM_VK_ALT = 18;
  static const uint32_t DOM_VK_PAUSE = 19;
  static const uint32_t DOM_VK_CAPS_LOCK = 20;
  static const uint32_t DOM_VK_KANA = 21;
  static const uint32_t DOM_VK_HANGUL = 21;
  static const uint32_t DOM_VK_EISU = 22;
  static const uint32_t DOM_VK_JUNJA = 23;
  static const uint32_t DOM_VK_FINAL = 24;
  static const uint32_t DOM_VK_HANJA = 25;
  static const uint32_t DOM_VK_KANJI = 25;
  static const uint32_t DOM_VK_ESCAPE = 27;
  static const uint32_t DOM_VK_CONVERT = 28;
  static const uint32_t DOM_VK_NONCONVERT = 29;
  static const uint32_t DOM_VK_ACCEPT = 30;
  static const uint32_t DOM_VK_MODECHANGE = 31;
  static const uint32_t DOM_VK_SPACE = 32;
  static const uint32_t DOM_VK_PAGE_UP = 33;
  static const uint32_t DOM_VK_PAGE_DOWN = 34;
  static const uint32_t DOM_VK_END = 35;
  static const uint32_t DOM_VK_HOME = 36;
  static const uint32_t DOM_VK_LEFT = 37;
  static const uint32_t DOM_VK_UP = 38;
  static const uint32_t DOM_VK_RIGHT = 39;
  static const uint32_t DOM_VK_DOWN = 40;
  static const uint32_t DOM_VK_SELECT = 41;
  static const uint32_t DOM_VK_PRINT = 42;
  static const uint32_t DOM_VK_EXECUTE = 43;
  static const uint32_t DOM_VK_PRINTSCREEN = 44;
  static const uint32_t DOM_VK_INSERT = 45;
  static const uint32_t DOM_VK_DELETE = 46;
  static const uint32_t DOM_VK_0 = 48;
  static const uint32_t DOM_VK_1 = 49;
  static const uint32_t DOM_VK_2 = 50;
  static const uint32_t DOM_VK_3 = 51;
  static const uint32_t DOM_VK_4 = 52;
  static const uint32_t DOM_VK_5 = 53;
  static const uint32_t DOM_VK_6 = 54;
  static const uint32_t DOM_VK_7 = 55;
  static const uint32_t DOM_VK_8 = 56;
  static const uint32_t DOM_VK_9 = 57;
  static const uint32_t DOM_VK_COLON = 58;
  static const uint32_t DOM_VK_SEMICOLON = 59;
  static const uint32_t DOM_VK_LESS_THAN = 60;
  static const uint32_t DOM_VK_EQUALS = 61;
  static const uint32_t DOM_VK_GREATER_THAN = 62;
  static const uint32_t DOM_VK_QUESTION_MARK = 63;
  static const uint32_t DOM_VK_AT = 64;
  static const uint32_t DOM_VK_A = 65;
  static const uint32_t DOM_VK_B = 66;
  static const uint32_t DOM_VK_C = 67;
  static const uint32_t DOM_VK_D = 68;
  static const uint32_t DOM_VK_E = 69;
  static const uint32_t DOM_VK_F = 70;
  static const uint32_t DOM_VK_G = 71;
  static const uint32_t DOM_VK_H = 72;
  static const uint32_t DOM_VK_I = 73;
  static const uint32_t DOM_VK_J = 74;
  static const uint32_t DOM_VK_K = 75;
  static const uint32_t DOM_VK_L = 76;
  static const uint32_t DOM_VK_M = 77;
  static const uint32_t DOM_VK_N = 78;
  static const uint32_t DOM_VK_O = 79;
  static const uint32_t DOM_VK_P = 80;
  static const uint32_t DOM_VK_Q = 81;
  static const uint32_t DOM_VK_R = 82;
  static const uint32_t DOM_VK_S = 83;
  static const uint32_t DOM_VK_T = 84;
  static const uint32_t DOM_VK_U = 85;
  static const uint32_t DOM_VK_V = 86;
  static const uint32_t DOM_VK_W = 87;
  static const uint32_t DOM_VK_X = 88;
  static const uint32_t DOM_VK_Y = 89;
  static const uint32_t DOM_VK_Z = 90;
  static const uint32_t DOM_VK_WIN = 91;
  static const uint32_t DOM_VK_CONTEXT_MENU = 93;
  static const uint32_t DOM_VK_SLEEP = 95;
  static const uint32_t DOM_VK_NUMPAD0 = 96;
  static const uint32_t DOM_VK_NUMPAD1 = 97;
  static const uint32_t DOM_VK_NUMPAD2 = 98;
  static const uint32_t DOM_VK_NUMPAD3 = 99;
  static const uint32_t DOM_VK_NUMPAD4 = 100;
  static const uint32_t DOM_VK_NUMPAD5 = 101;
  static const uint32_t DOM_VK_NUMPAD6 = 102;
  static const uint32_t DOM_VK_NUMPAD7 = 103;
  static const uint32_t DOM_VK_NUMPAD8 = 104;
  static const uint32_t DOM_VK_NUMPAD9 = 105;
  static const uint32_t DOM_VK_MULTIPLY = 106;
  static const uint32_t DOM_VK_ADD = 107;
  static const uint32_t DOM_VK_SEPARATOR = 108;
  static const uint32_t DOM_VK_SUBTRACT = 109;
  static const uint32_t DOM_VK_DECIMAL = 110;
  static const uint32_t DOM_VK_DIVIDE = 111;
  static const uint32_t DOM_VK_F1 = 112;
  static const uint32_t DOM_VK_F2 = 113;
  static const uint32_t DOM_VK_F3 = 114;
  static const uint32_t DOM_VK_F4 = 115;
  static const uint32_t DOM_VK_F5 = 116;
  static const uint32_t DOM_VK_F6 = 117;
  static const uint32_t DOM_VK_F7 = 118;
  static const uint32_t DOM_VK_F8 = 119;
  static const uint32_t DOM_VK_F9 = 120;
  static const uint32_t DOM_VK_F10 = 121;
  static const uint32_t DOM_VK_F11 = 122;
  static const uint32_t DOM_VK_F12 = 123;
  static const uint32_t DOM_VK_F13 = 124;
  static const uint32_t DOM_VK_F14 = 125;
  static const uint32_t DOM_VK_F15 = 126;
  static const uint32_t DOM_VK_F16 = 127;
  static const uint32_t DOM_VK_F17 = 128;
  static const uint32_t DOM_VK_F18 = 129;
  static const uint32_t DOM_VK_F19 = 130;
  static const uint32_t DOM_VK_F20 = 131;
  static const uint32_t DOM_VK_F21 = 132;
  static const uint32_t DOM_VK_F22 = 133;
  static const uint32_t DOM_VK_F23 = 134;
  static const uint32_t DOM_VK_F24 = 135;
  static const uint32_t DOM_VK_NUM_LOCK = 144;
  static const uint32_t DOM_VK_SCROLL_LOCK = 145;
  static const uint32_t DOM_VK_WIN_OEM_FJ_JISHO = 146;
  static const uint32_t DOM_VK_WIN_OEM_FJ_MASSHOU = 147;
  static const uint32_t DOM_VK_WIN_OEM_FJ_TOUROKU = 148;
  static const uint32_t DOM_VK_WIN_OEM_FJ_LOYA = 149;
  static const uint32_t DOM_VK_WIN_OEM_FJ_ROYA = 150;
  static const uint32_t DOM_VK_CIRCUMFLEX = 160;
  static const uint32_t DOM_VK_EXCLAMATION = 161;
  static const uint32_t DOM_VK_DOUBLE_QUOTE = 162;
  static const uint32_t DOM_VK_HASH = 163;
  static const uint32_t DOM_VK_DOLLAR = 164;
  static const uint32_t DOM_VK_PERCENT = 165;
  static const uint32_t DOM_VK_AMPERSAND = 166;
  static const uint32_t DOM_VK_UNDERSCORE = 167;
  static const uint32_t DOM_VK_OPEN_PAREN = 168;
  static const uint32_t DOM_VK_CLOSE_PAREN = 169;
  static const uint32_t DOM_VK_ASTERISK = 170;
  static const uint32_t DOM_VK_PLUS = 171;
  static const uint32_t DOM_VK_PIPE = 172;
  static const uint32_t DOM_VK_HYPHEN_MINUS = 173;
  static const uint32_t DOM_VK_OPEN_CURLY_BRACKET = 174;
  static const uint32_t DOM_VK_CLOSE_CURLY_BRACKET = 175;
  static const uint32_t DOM_VK_TILDE = 176;
  static const uint32_t DOM_VK_VOLUME_MUTE = 181;
  static const uint32_t DOM_VK_VOLUME_DOWN = 182;
  static const uint32_t DOM_VK_VOLUME_UP = 183;
  static const uint32_t DOM_VK_COMMA = 188;
  static const uint32_t DOM_VK_PERIOD = 190;
  static const uint32_t DOM_VK_SLASH = 191;
  static const uint32_t DOM_VK_BACK_QUOTE = 192;
  static const uint32_t DOM_VK_OPEN_BRACKET = 219;
  static const uint32_t DOM_VK_BACK_SLASH = 220;
  static const uint32_t DOM_VK_CLOSE_BRACKET = 221;
  static const uint32_t DOM_VK_QUOTE = 222;
  static const uint32_t DOM_VK_META = 224;
  static const uint32_t DOM_VK_ALTGR = 225;
  static const uint32_t DOM_VK_WIN_ICO_HELP = 227;
  static const uint32_t DOM_VK_WIN_ICO_00 = 228;
  static const uint32_t DOM_VK_WIN_ICO_CLEAR = 230;
  static const uint32_t DOM_VK_WIN_OEM_RESET = 233;
  static const uint32_t DOM_VK_WIN_OEM_JUMP = 234;
  static const uint32_t DOM_VK_WIN_OEM_PA1 = 235;
  static const uint32_t DOM_VK_WIN_OEM_PA2 = 236;
  static const uint32_t DOM_VK_WIN_OEM_PA3 = 237;
  static const uint32_t DOM_VK_WIN_OEM_WSCTRL = 238;
  static const uint32_t DOM_VK_WIN_OEM_CUSEL = 239;
  static const uint32_t DOM_VK_WIN_OEM_ATTN = 240;
  static const uint32_t DOM_VK_WIN_OEM_FINISH = 241;
  static const uint32_t DOM_VK_WIN_OEM_COPY = 242;
  static const uint32_t DOM_VK_WIN_OEM_AUTO = 243;
  static const uint32_t DOM_VK_WIN_OEM_ENLW = 244;
  static const uint32_t DOM_VK_WIN_OEM_BACKTAB = 245;
  static const uint32_t DOM_VK_ATTN = 246;
  static const uint32_t DOM_VK_CRSEL = 247;
  static const uint32_t DOM_VK_EXSEL = 248;
  static const uint32_t DOM_VK_EREOF = 249;
  static const uint32_t DOM_VK_PLAY = 250;
  static const uint32_t DOM_VK_ZOOM = 251;
  static const uint32_t DOM_VK_PA1 = 253;
  static const uint32_t DOM_VK_WIN_OEM_CLEAR = 254;
}
}
}

using namespace mozilla;

static struct nsKeyConverter nsKeycodes[] = {
//  { dom::KeyboardEventBinding::DOM_VK_CANCEL,        Qt::Key_Cancel },
    { dom::KeyboardEventBinding::DOM_VK_BACK_SPACE,    Qt::Key_Backspace },
    { dom::KeyboardEventBinding::DOM_VK_TAB,           Qt::Key_Tab },
    { dom::KeyboardEventBinding::DOM_VK_TAB,           Qt::Key_Backtab },
//  { dom::KeyboardEventBinding::DOM_VK_CLEAR,         Qt::Key_Clear },
    { dom::KeyboardEventBinding::DOM_VK_RETURN,        Qt::Key_Return },
    { dom::KeyboardEventBinding::DOM_VK_RETURN,        Qt::Key_Enter },
    { dom::KeyboardEventBinding::DOM_VK_SHIFT,         Qt::Key_Shift },
    { dom::KeyboardEventBinding::DOM_VK_CONTROL,       Qt::Key_Control },
    { dom::KeyboardEventBinding::DOM_VK_ALT,           Qt::Key_Alt },
    { dom::KeyboardEventBinding::DOM_VK_PAUSE,         Qt::Key_Pause },
    { dom::KeyboardEventBinding::DOM_VK_CAPS_LOCK,     Qt::Key_CapsLock },
    { dom::KeyboardEventBinding::DOM_VK_ESCAPE,        Qt::Key_Escape },
    { dom::KeyboardEventBinding::DOM_VK_SPACE,         Qt::Key_Space },
    { dom::KeyboardEventBinding::DOM_VK_PAGE_UP,       Qt::Key_PageUp },
    { dom::KeyboardEventBinding::DOM_VK_PAGE_DOWN,     Qt::Key_PageDown },
    { dom::KeyboardEventBinding::DOM_VK_END,           Qt::Key_End },
    { dom::KeyboardEventBinding::DOM_VK_HOME,          Qt::Key_Home },
    { dom::KeyboardEventBinding::DOM_VK_LEFT,          Qt::Key_Left },
    { dom::KeyboardEventBinding::DOM_VK_UP,            Qt::Key_Up },
    { dom::KeyboardEventBinding::DOM_VK_RIGHT,         Qt::Key_Right },
    { dom::KeyboardEventBinding::DOM_VK_DOWN,          Qt::Key_Down },
    { dom::KeyboardEventBinding::DOM_VK_PRINTSCREEN,   Qt::Key_Print },
    { dom::KeyboardEventBinding::DOM_VK_INSERT,        Qt::Key_Insert },
    { dom::KeyboardEventBinding::DOM_VK_DELETE,        Qt::Key_Delete },
    { dom::KeyboardEventBinding::DOM_VK_HELP,          Qt::Key_Help },

    { dom::KeyboardEventBinding::DOM_VK_0,             Qt::Key_0 },
    { dom::KeyboardEventBinding::DOM_VK_1,             Qt::Key_1 },
    { dom::KeyboardEventBinding::DOM_VK_2,             Qt::Key_2 },
    { dom::KeyboardEventBinding::DOM_VK_3,             Qt::Key_3 },
    { dom::KeyboardEventBinding::DOM_VK_4,             Qt::Key_4 },
    { dom::KeyboardEventBinding::DOM_VK_5,             Qt::Key_5 },
    { dom::KeyboardEventBinding::DOM_VK_6,             Qt::Key_6 },
    { dom::KeyboardEventBinding::DOM_VK_7,             Qt::Key_7 },
    { dom::KeyboardEventBinding::DOM_VK_8,             Qt::Key_8 },
    { dom::KeyboardEventBinding::DOM_VK_9,             Qt::Key_9 },

    { dom::KeyboardEventBinding::DOM_VK_SEMICOLON,     Qt::Key_Semicolon },
    { dom::KeyboardEventBinding::DOM_VK_EQUALS,        Qt::Key_Equal },

    { dom::KeyboardEventBinding::DOM_VK_A,             Qt::Key_A },
    { dom::KeyboardEventBinding::DOM_VK_B,             Qt::Key_B },
    { dom::KeyboardEventBinding::DOM_VK_C,             Qt::Key_C },
    { dom::KeyboardEventBinding::DOM_VK_D,             Qt::Key_D },
    { dom::KeyboardEventBinding::DOM_VK_E,             Qt::Key_E },
    { dom::KeyboardEventBinding::DOM_VK_F,             Qt::Key_F },
    { dom::KeyboardEventBinding::DOM_VK_G,             Qt::Key_G },
    { dom::KeyboardEventBinding::DOM_VK_H,             Qt::Key_H },
    { dom::KeyboardEventBinding::DOM_VK_I,             Qt::Key_I },
    { dom::KeyboardEventBinding::DOM_VK_J,             Qt::Key_J },
    { dom::KeyboardEventBinding::DOM_VK_K,             Qt::Key_K },
    { dom::KeyboardEventBinding::DOM_VK_L,             Qt::Key_L },
    { dom::KeyboardEventBinding::DOM_VK_M,             Qt::Key_M },
    { dom::KeyboardEventBinding::DOM_VK_N,             Qt::Key_N },
    { dom::KeyboardEventBinding::DOM_VK_O,             Qt::Key_O },
    { dom::KeyboardEventBinding::DOM_VK_P,             Qt::Key_P },
    { dom::KeyboardEventBinding::DOM_VK_Q,             Qt::Key_Q },
    { dom::KeyboardEventBinding::DOM_VK_R,             Qt::Key_R },
    { dom::KeyboardEventBinding::DOM_VK_S,             Qt::Key_S },
    { dom::KeyboardEventBinding::DOM_VK_T,             Qt::Key_T },
    { dom::KeyboardEventBinding::DOM_VK_U,             Qt::Key_U },
    { dom::KeyboardEventBinding::DOM_VK_V,             Qt::Key_V },
    { dom::KeyboardEventBinding::DOM_VK_W,             Qt::Key_W },
    { dom::KeyboardEventBinding::DOM_VK_X,             Qt::Key_X },
    { dom::KeyboardEventBinding::DOM_VK_Y,             Qt::Key_Y },
    { dom::KeyboardEventBinding::DOM_VK_Z,             Qt::Key_Z },

    { dom::KeyboardEventBinding::DOM_VK_NUMPAD0,       Qt::Key_0 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD1,       Qt::Key_1 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD2,       Qt::Key_2 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD3,       Qt::Key_3 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD4,       Qt::Key_4 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD5,       Qt::Key_5 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD6,       Qt::Key_6 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD7,       Qt::Key_7 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD8,       Qt::Key_8 },
    { dom::KeyboardEventBinding::DOM_VK_NUMPAD9,       Qt::Key_9 },
    { dom::KeyboardEventBinding::DOM_VK_MULTIPLY,      Qt::Key_Asterisk },
    { dom::KeyboardEventBinding::DOM_VK_ADD,           Qt::Key_Plus },
//  { dom::KeyboardEventBinding::DOM_VK_SEPARATOR,     Qt::Key_Separator },
    { dom::KeyboardEventBinding::DOM_VK_SUBTRACT,      Qt::Key_Minus },
    { dom::KeyboardEventBinding::DOM_VK_DECIMAL,       Qt::Key_Period },
    { dom::KeyboardEventBinding::DOM_VK_DIVIDE,        Qt::Key_Slash },
    { dom::KeyboardEventBinding::DOM_VK_F1,            Qt::Key_F1 },
    { dom::KeyboardEventBinding::DOM_VK_F2,            Qt::Key_F2 },
    { dom::KeyboardEventBinding::DOM_VK_F3,            Qt::Key_F3 },
    { dom::KeyboardEventBinding::DOM_VK_F4,            Qt::Key_F4 },
    { dom::KeyboardEventBinding::DOM_VK_F5,            Qt::Key_F5 },
    { dom::KeyboardEventBinding::DOM_VK_F6,            Qt::Key_F6 },
    { dom::KeyboardEventBinding::DOM_VK_F7,            Qt::Key_F7 },
    { dom::KeyboardEventBinding::DOM_VK_F8,            Qt::Key_F8 },
    { dom::KeyboardEventBinding::DOM_VK_F9,            Qt::Key_F9 },
    { dom::KeyboardEventBinding::DOM_VK_F10,           Qt::Key_F10 },
    { dom::KeyboardEventBinding::DOM_VK_F11,           Qt::Key_F11 },
    { dom::KeyboardEventBinding::DOM_VK_F12,           Qt::Key_F12 },
    { dom::KeyboardEventBinding::DOM_VK_F13,           Qt::Key_F13 },
    { dom::KeyboardEventBinding::DOM_VK_F14,           Qt::Key_F14 },
    { dom::KeyboardEventBinding::DOM_VK_F15,           Qt::Key_F15 },
    { dom::KeyboardEventBinding::DOM_VK_F16,           Qt::Key_F16 },
    { dom::KeyboardEventBinding::DOM_VK_F17,           Qt::Key_F17 },
    { dom::KeyboardEventBinding::DOM_VK_F18,           Qt::Key_F18 },
    { dom::KeyboardEventBinding::DOM_VK_F19,           Qt::Key_F19 },
    { dom::KeyboardEventBinding::DOM_VK_F20,           Qt::Key_F20 },
    { dom::KeyboardEventBinding::DOM_VK_F21,           Qt::Key_F21 },
    { dom::KeyboardEventBinding::DOM_VK_F22,           Qt::Key_F22 },
    { dom::KeyboardEventBinding::DOM_VK_F23,           Qt::Key_F23 },
    { dom::KeyboardEventBinding::DOM_VK_F24,           Qt::Key_F24 },

    { dom::KeyboardEventBinding::DOM_VK_NUM_LOCK,      Qt::Key_NumLock },
    { dom::KeyboardEventBinding::DOM_VK_SCROLL_LOCK,   Qt::Key_ScrollLock },

    { dom::KeyboardEventBinding::DOM_VK_COMMA,         Qt::Key_Comma },
    { dom::KeyboardEventBinding::DOM_VK_PERIOD,        Qt::Key_Period },
    { dom::KeyboardEventBinding::DOM_VK_SLASH,         Qt::Key_Slash },
    { dom::KeyboardEventBinding::DOM_VK_BACK_QUOTE,    Qt::Key_QuoteLeft },
    { dom::KeyboardEventBinding::DOM_VK_OPEN_BRACKET,  Qt::Key_ParenLeft },
    { dom::KeyboardEventBinding::DOM_VK_CLOSE_BRACKET, Qt::Key_ParenRight },
    { dom::KeyboardEventBinding::DOM_VK_QUOTE,         Qt::Key_QuoteDbl },

    { dom::KeyboardEventBinding::DOM_VK_META,          Qt::Key_Meta }
};

int
MozKey::QtKeyCodeToDOMKeyCode(int aKeysym, int aModifier)
{
    unsigned int i;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    // since X has different key symbols for upper and lowercase letters and
    // mozilla does not, convert gdk's to mozilla's
    if (aKeysym >= Qt::Key_A && aKeysym <= Qt::Key_Z)
        return aKeysym - Qt::Key_A + dom::KeyboardEventBinding::DOM_VK_A;

    // numbers
    if (aKeysym >= Qt::Key_0 && aKeysym <= Qt::Key_9)
        return aKeysym - Qt::Key_0 + dom::KeyboardEventBinding::DOM_VK_0;

    // keypad numbers
    if (aKeysym >= Qt::Key_0 && aKeysym <= Qt::Key_9 && aModifier & Qt::KeypadModifier)
        return aKeysym - Qt::Key_0 + dom::KeyboardEventBinding::DOM_VK_NUMPAD0;

    // misc other things
    for (i = 0; i < ArrayLength(nsKeycodes); i++) {
        if (nsKeycodes[i].keysym == aKeysym)
            return (nsKeycodes[i].vkCode);
    }

    // function keys
    if (aKeysym >= Qt::Key_F1 && aKeysym <= Qt::Key_F24)
        return aKeysym - Qt::Key_F1 + dom::KeyboardEventBinding::DOM_VK_F1;

    return ((int)0);
}

int
MozKey::DOMKeyCodeToQtKeyCode(int aKeysym)
{
    unsigned int i;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    if (aKeysym >= dom::KeyboardEventBinding::DOM_VK_A && aKeysym <= dom::KeyboardEventBinding::DOM_VK_Z) {
        // gdk and DOM both use the ASCII codes for these keys.
        return aKeysym;
    }

    // numbers
    if (aKeysym >= dom::KeyboardEventBinding::DOM_VK_0 && aKeysym <= dom::KeyboardEventBinding::DOM_VK_9) {
        // gdk and DOM both use the ASCII codes for these keys.
        return aKeysym - Qt::Key_0 + dom::KeyboardEventBinding::DOM_VK_0;
    }

    // keypad numbers
    if (aKeysym >= dom::KeyboardEventBinding::DOM_VK_NUMPAD0 && aKeysym <= dom::KeyboardEventBinding::DOM_VK_NUMPAD9) {
        NS_ERROR("keypad numbers conversion not implemented");
        //return aKeysym - dom::KeyboardEventBinding::DOM_VK_NUMPAD0 + Qt::Key_KP_0;
        return 0;
    }

    // misc other things
    for (i = 0; i < ArrayLength(nsKeycodes); ++i) {
        if (nsKeycodes[i].vkCode == aKeysym) {
            return nsKeycodes[i].keysym;
        }
    }

    // function keys
    if (aKeysym >= dom::KeyboardEventBinding::DOM_VK_F1 && aKeysym <= dom::KeyboardEventBinding::DOM_VK_F9)
        return aKeysym - dom::KeyboardEventBinding::DOM_VK_F1 + Qt::Key_F1;

    return 0;
}

uint32_t *
MozKey::GetFlagWord32(uint32_t aKeyCode, uint32_t *aMask)
{
    /* Mozilla DOM Virtual Key Code is from 0 to 224. */
    // NS_ASSERTION((aKeyCode <= 0xFF), "Invalid DOM Key Code");
    static uint32_t mKeyDownFlags[8];
    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));
    aKeyCode &= 0xFF;

    /* 32 = 2^5 = 0x20 */
    *aMask = uint32_t(1) << (aKeyCode & 0x1F);
    return &mKeyDownFlags[(aKeyCode >> 5)];
}

bool
MozKey::IsKeyDown(uint32_t aKeyCode)
{
    uint32_t mask;
    uint32_t *flag = GetFlagWord32(aKeyCode, &mask);
    return ((*flag) & mask) != 0;
}

void
MozKey::SetKeyDownFlag(uint32_t aKeyCode)
{
    uint32_t mask;
    uint32_t *flag = GetFlagWord32(aKeyCode, &mask);
    *flag |= mask;
}

void
MozKey::ClearKeyDownFlag(uint32_t aKeyCode)
{
    uint32_t mask;
    uint32_t *flag = GetFlagWord32(aKeyCode, &mask);
    *flag &= ~mask;
}

int
MozKey::QtModifierToDOMModifier(int aModifiers)
{
    int32_t modifiers = 0;
    if (aModifiers & Qt::ControlModifier) {
        modifiers |= nsIDOMWindowUtils::MODIFIER_CONTROL;
    }
    if (aModifiers & Qt::AltModifier) {
        modifiers |= nsIDOMWindowUtils::MODIFIER_ALT;
    }
    if (aModifiers & Qt::ShiftModifier) {
        modifiers |= nsIDOMWindowUtils::MODIFIER_SHIFT;
    }
    if (aModifiers & Qt::MetaModifier) {
        modifiers |= nsIDOMWindowUtils::MODIFIER_META;
    }
    return modifiers;
}
