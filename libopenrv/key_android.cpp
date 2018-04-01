#define XK_MISCELLANY
#define XK_XKB_KEYS
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4
#define XK_LATIN8
#include "keysymdef.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic push
#endif

// keycodes as defined in KeyEvent.java
// FIXME: are these keycodes constant in various android versions?
static const int KEYCODE_UNKNOWN = 0;
static const int KEYCODE_SOFT_LEFT = 1;
static const int KEYCODE_SOFT_RIGHT = 2;
static const int KEYCODE_HOME = 3;
static const int KEYCODE_BACK = 4;
static const int KEYCODE_CALL = 5;
static const int KEYCODE_ENDCALL = 6;
static const int KEYCODE_0 = 7;
static const int KEYCODE_1 = 8;
static const int KEYCODE_2 = 9;
static const int KEYCODE_3 = 10;
static const int KEYCODE_4 = 11;
static const int KEYCODE_5 = 12;
static const int KEYCODE_6 = 13;
static const int KEYCODE_7 = 14;
static const int KEYCODE_8 = 15;
static const int KEYCODE_9 = 16; 
static const int KEYCODE_STAR = 17;
static const int KEYCODE_POUND = 18;
static const int KEYCODE_DPAD_UP = 19;
static const int KEYCODE_DPAD_DOWN = 20;
static const int KEYCODE_DPAD_LEFT = 21;
static const int KEYCODE_DPAD_RIGHT = 22;
static const int KEYCODE_DPAD_CENTER = 23;
static const int KEYCODE_VOLUME_UP = 24;
static const int KEYCODE_VOLUME_DOWN = 25;
static const int KEYCODE_POWER = 26;
static const int KEYCODE_CAMERA = 27;
static const int KEYCODE_CLEAR = 28;
static const int KEYCODE_A = 29;
static const int KEYCODE_B = 30;
static const int KEYCODE_C = 31;
static const int KEYCODE_D = 32;
static const int KEYCODE_E = 33;
static const int KEYCODE_F = 34;
static const int KEYCODE_G = 35;
static const int KEYCODE_H = 36;
static const int KEYCODE_I = 37;
static const int KEYCODE_J = 38;
static const int KEYCODE_K = 39;
static const int KEYCODE_L = 40;
static const int KEYCODE_M = 41;
static const int KEYCODE_N = 42;
static const int KEYCODE_O = 43;
static const int KEYCODE_P = 44;
static const int KEYCODE_Q = 45;
static const int KEYCODE_R = 46;
static const int KEYCODE_S = 47;
static const int KEYCODE_T = 48;
static const int KEYCODE_U = 49;
static const int KEYCODE_V = 50;
static const int KEYCODE_W = 51;
static const int KEYCODE_X = 52;
static const int KEYCODE_Y = 53;
static const int KEYCODE_Z = 54;
static const int KEYCODE_COMMA = 55;
static const int KEYCODE_PERIOD = 56;
static const int KEYCODE_ALT_LEFT = 57;
static const int KEYCODE_ALT_RIGHT = 58;
static const int KEYCODE_SHIFT_LEFT = 59;
static const int KEYCODE_SHIFT_RIGHT = 60;
static const int KEYCODE_TAB = 61;
static const int KEYCODE_SPACE = 62;
static const int KEYCODE_SYM = 63;
static const int KEYCODE_EXPLORER = 64;
static const int KEYCODE_ENVELOPE = 65;
static const int KEYCODE_ENTER = 66;
static const int KEYCODE_DEL = 67;
static const int KEYCODE_GRAVE = 68;
static const int KEYCODE_MINUS = 69;
static const int KEYCODE_EQUALS = 70;
static const int KEYCODE_LEFT_BRACKET = 71;
static const int KEYCODE_RIGHT_BRACKET = 72;
static const int KEYCODE_BACKSLASH = 73;
static const int KEYCODE_SEMICOLON = 74;
static const int KEYCODE_APOSTROPHE = 75;
static const int KEYCODE_SLASH = 76;
static const int KEYCODE_AT = 77;
static const int KEYCODE_NUM = 78;
static const int KEYCODE_HEADSETHOOK = 79;
static const int KEYCODE_FOCUS = 80;   // *Camera* focus
static const int KEYCODE_PLUS = 81;
static const int KEYCODE_MENU = 82;
static const int KEYCODE_NOTIFICATION = 83;
static const int KEYCODE_SEARCH = 84;
static const int KEYCODE_MEDIA_PLAY_PAUSE= 85;
static const int KEYCODE_MEDIA_STOP = 86;
static const int KEYCODE_MEDIA_NEXT = 87;
static const int KEYCODE_MEDIA_PREVIOUS = 88;
static const int KEYCODE_MEDIA_REWIND = 89;
static const int KEYCODE_MEDIA_FAST_FORWARD = 90;
static const int KEYCODE_MUTE = 91;
static const int KEYCODE_PAGE_UP = 92;
static const int KEYCODE_PAGE_DOWN = 93;
static const int KEYCODE_PICTSYMBOLS = 94;   // switch symbol-sets (Emoji,Kao-moji)
static const int KEYCODE_SWITCH_CHARSET = 95;   // switch char-sets (Kanji,Katakana)
static const int KEYCODE_BUTTON_A = 96;
static const int KEYCODE_BUTTON_B = 97;
static const int KEYCODE_BUTTON_C = 98;
static const int KEYCODE_BUTTON_X = 99;
static const int KEYCODE_BUTTON_Y = 100;
static const int KEYCODE_BUTTON_Z = 101;
static const int KEYCODE_BUTTON_L1 = 102;
static const int KEYCODE_BUTTON_R1 = 103;
static const int KEYCODE_BUTTON_L2 = 104;
static const int KEYCODE_BUTTON_R2 = 105;
static const int KEYCODE_BUTTON_THUMBL = 106;
static const int KEYCODE_BUTTON_THUMBR = 107;
static const int KEYCODE_BUTTON_START = 108;
static const int KEYCODE_BUTTON_SELECT = 109;
static const int KEYCODE_BUTTON_MODE = 110;
static const int KEYCODE_ESCAPE = 111;
static const int KEYCODE_FORWARD_DEL = 112;
static const int KEYCODE_CTRL_LEFT = 113;
static const int KEYCODE_CTRL_RIGHT = 114;
static const int KEYCODE_CAPS_LOCK = 115;
static const int KEYCODE_SCROLL_LOCK = 116;
static const int KEYCODE_META_LEFT = 117;
static const int KEYCODE_META_RIGHT = 118;
static const int KEYCODE_FUNCTION = 119;
static const int KEYCODE_SYSRQ = 120;
static const int KEYCODE_BREAK = 121;
static const int KEYCODE_MOVE_HOME = 122;
static const int KEYCODE_MOVE_END = 123;
static const int KEYCODE_INSERT = 124;
static const int KEYCODE_FORWARD = 125;
static const int KEYCODE_MEDIA_PLAY = 126;
static const int KEYCODE_MEDIA_PAUSE = 127;
static const int KEYCODE_MEDIA_CLOSE = 128;
static const int KEYCODE_MEDIA_EJECT = 129;
static const int KEYCODE_MEDIA_RECORD = 130;
static const int KEYCODE_F1 = 131;
static const int KEYCODE_F2 = 132;
static const int KEYCODE_F3 = 133;
static const int KEYCODE_F4 = 134;
static const int KEYCODE_F5 = 135;
static const int KEYCODE_F6 = 136;
static const int KEYCODE_F7 = 137;
static const int KEYCODE_F8 = 138;
static const int KEYCODE_F9 = 139;
static const int KEYCODE_F10 = 140;
static const int KEYCODE_F11 = 141;
static const int KEYCODE_F12 = 142;
static const int KEYCODE_NUM_LOCK = 143;
static const int KEYCODE_NUMPAD_0 = 144;
static const int KEYCODE_NUMPAD_1 = 145;
static const int KEYCODE_NUMPAD_2 = 146;
static const int KEYCODE_NUMPAD_3 = 147;
static const int KEYCODE_NUMPAD_4 = 148;
static const int KEYCODE_NUMPAD_5 = 149;
static const int KEYCODE_NUMPAD_6 = 150;
static const int KEYCODE_NUMPAD_7 = 151;
static const int KEYCODE_NUMPAD_8 = 152;
static const int KEYCODE_NUMPAD_9 = 153;
static const int KEYCODE_NUMPAD_DIVIDE = 154;
static const int KEYCODE_NUMPAD_MULTIPLY = 155;
static const int KEYCODE_NUMPAD_SUBTRACT = 156;
static const int KEYCODE_NUMPAD_ADD = 157;
static const int KEYCODE_NUMPAD_DOT = 158;
static const int KEYCODE_NUMPAD_COMMA = 159;
static const int KEYCODE_NUMPAD_ENTER = 160;
static const int KEYCODE_NUMPAD_EQUALS = 161;
static const int KEYCODE_NUMPAD_LEFT_PAREN = 162;
static const int KEYCODE_NUMPAD_RIGHT_PAREN = 163;
static const int KEYCODE_VOLUME_MUTE = 164;
static const int KEYCODE_INFO = 165;
static const int KEYCODE_CHANNEL_UP = 166;
static const int KEYCODE_CHANNEL_DOWN = 167;
static const int KEYCODE_ZOOM_IN = 168;
static const int KEYCODE_ZOOM_OUT = 169;
static const int KEYCODE_TV = 170;
static const int KEYCODE_WINDOW = 171;
static const int KEYCODE_GUIDE = 172;
static const int KEYCODE_DVR = 173;
static const int KEYCODE_BOOKMARK = 174;
static const int KEYCODE_CAPTIONS = 175;
static const int KEYCODE_SETTINGS = 176;
static const int KEYCODE_TV_POWER = 177;
static const int KEYCODE_TV_INPUT = 178;
static const int KEYCODE_STB_POWER = 179;
static const int KEYCODE_STB_INPUT = 180;
static const int KEYCODE_AVR_POWER = 181;
static const int KEYCODE_AVR_INPUT = 182;
static const int KEYCODE_PROG_RED = 183;
static const int KEYCODE_PROG_GREEN = 184;
static const int KEYCODE_PROG_YELLOW = 185;
static const int KEYCODE_PROG_BLUE = 186;
static const int KEYCODE_APP_SWITCH = 187;
static const int KEYCODE_BUTTON_1 = 188;
static const int KEYCODE_BUTTON_2 = 189;
static const int KEYCODE_BUTTON_3 = 190;
static const int KEYCODE_BUTTON_4 = 191;
static const int KEYCODE_BUTTON_5 = 192;
static const int KEYCODE_BUTTON_6 = 193;
static const int KEYCODE_BUTTON_7 = 194;
static const int KEYCODE_BUTTON_8 = 195;
static const int KEYCODE_BUTTON_9 = 196;
static const int KEYCODE_BUTTON_10 = 197;
static const int KEYCODE_BUTTON_11 = 198;
static const int KEYCODE_BUTTON_12 = 199;
static const int KEYCODE_BUTTON_13 = 200;
static const int KEYCODE_BUTTON_14 = 201;
static const int KEYCODE_BUTTON_15 = 202;
static const int KEYCODE_BUTTON_16 = 203;
static const int KEYCODE_LANGUAGE_SWITCH = 204;
static const int KEYCODE_MANNER_MODE = 205;
static const int KEYCODE_3D_MODE = 206;
static const int KEYCODE_CONTACTS = 207;
static const int KEYCODE_CALENDAR = 208;
static const int KEYCODE_MUSIC = 209;
static const int KEYCODE_CALCULATOR = 210;
static const int KEYCODE_ZENKAKU_HANKAKU = 211;
static const int KEYCODE_EISU = 212;
static const int KEYCODE_MUHENKAN = 213;
static const int KEYCODE_HENKAN = 214;
static const int KEYCODE_KATAKANA_HIRAGANA = 215;
static const int KEYCODE_YEN = 216;
static const int KEYCODE_RO = 217;
static const int KEYCODE_KANA = 218;
static const int KEYCODE_ASSIST = 219;
static const int KEYCODE_BRIGHTNESS_DOWN = 220;
static const int KEYCODE_BRIGHTNESS_UP = 221;
static const int KEYCODE_MEDIA_AUDIO_TRACK = 222;
static const int KEYCODE_SLEEP = 223;
static const int KEYCODE_WAKEUP = 224;
static const int KEYCODE_PAIRING = 225;
static const int KEYCODE_MEDIA_TOP_MENU = 226;
static const int KEYCODE_11 = 227;
static const int KEYCODE_12 = 228;
static const int KEYCODE_LAST_CHANNEL = 229;
static const int KEYCODE_TV_DATA_SERVICE = 230;
static const int KEYCODE_VOICE_ASSIST = 231;
static const int KEYCODE_TV_RADIO_SERVICE = 232;
static const int KEYCODE_TV_TELETEXT = 233;
static const int KEYCODE_TV_NUMBER_ENTRY = 234;
static const int KEYCODE_TV_TERRESTRIAL_ANALOG = 235;
static const int KEYCODE_TV_TERRESTRIAL_DIGITAL = 236;
static const int KEYCODE_TV_SATELLITE = 237;
static const int KEYCODE_TV_SATELLITE_BS = 238;
static const int KEYCODE_TV_SATELLITE_CS = 239;
static const int KEYCODE_TV_SATELLITE_SERVICE = 240;
static const int KEYCODE_TV_NETWORK = 241;
static const int KEYCODE_TV_ANTENNA_CABLE = 242;
static const int KEYCODE_TV_INPUT_HDMI_1 = 243;
static const int KEYCODE_TV_INPUT_HDMI_2 = 244;
static const int KEYCODE_TV_INPUT_HDMI_3 = 245;
static const int KEYCODE_TV_INPUT_HDMI_4 = 246;
static const int KEYCODE_TV_INPUT_COMPOSITE_1 = 247;
static const int KEYCODE_TV_INPUT_COMPOSITE_2 = 248;
static const int KEYCODE_TV_INPUT_COMPONENT_1 = 249;
static const int KEYCODE_TV_INPUT_COMPONENT_2 = 250;
static const int KEYCODE_TV_INPUT_VGA_1 = 251;
static const int KEYCODE_TV_AUDIO_DESCRIPTION = 252;
static const int KEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP = 253;
static const int KEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN = 254;
static const int KEYCODE_TV_ZOOM_MODE = 255;
static const int KEYCODE_TV_CONTENTS_MENU = 256;
static const int KEYCODE_TV_MEDIA_CONTEXT_MENU = 257;
static const int KEYCODE_TV_TIMER_PROGRAMMING = 258;
static const int KEYCODE_HELP = 259;
static const int KEYCODE_NAVIGATE_PREVIOUS = 260;
static const int KEYCODE_NAVIGATE_NEXT = 261;
static const int KEYCODE_NAVIGATE_IN = 262;
static const int KEYCODE_NAVIGATE_OUT = 263;
static const int KEYCODE_MEDIA_SKIP_FORWARD = 272;
static const int KEYCODE_MEDIA_SKIP_BACKWARD = 273;
static const int KEYCODE_MEDIA_STEP_FORWARD = 274;
static const int KEYCODE_MEDIA_STEP_BACKWARD = 275;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// meta flags as defines in KeyEvent.java
static const int META_ALT_ON = 0x02;
static const int META_ALT_LEFT_ON = 0x10;
static const int META_ALT_RIGHT_ON = 0x20;
static const int META_SHIFT_ON = 0x1;
static const int META_SHIFT_LEFT_ON = 0x40;
static const int META_SHIFT_RIGHT_ON = 0x80;
static const int META_SYM_ON = 0x4;
static const int META_FUNCTION_ON = 0x8;
static const int META_CTRL_ON = 0x1000;
static const int META_CTRL_LEFT_ON = 0x2000;
static const int META_CTRL_RIGHT_ON = 0x4000;
static const int META_META_ON = 0x10000;
static const int META_META_LEFT_ON = 0x20000;
static const int META_META_RIGHT_ON = 0x40000;
static const int META_CAPS_LOCK_ON = 0x100000;
static const int META_NUM_LOCK_ON = 0x200000;
static const int META_SCROLL_LOCK_ON = 0x400000;
static const int META_SHIFT_MASK = META_SHIFT_ON | META_SHIFT_LEFT_ON | META_SHIFT_RIGHT_ON;
static const int META_ALT_MASK = META_ALT_ON | META_ALT_LEFT_ON | META_ALT_RIGHT_ON;
static const int META_CTRL_MASK = META_CTRL_ON | META_CTRL_LEFT_ON | META_CTRL_RIGHT_ON;
static const int META_META_MASK = META_META_ON | META_META_LEFT_ON | META_META_RIGHT_ON;

int androidKeyEventToXKeyCode(int keycode, int metastate)
{
    if (keycode >= KEYCODE_A && keycode <= KEYCODE_Z)
    {
        int base = keycode - KEYCODE_A;
        if (metastate & META_SHIFT_ON)
        {
            return XK_A + base;
        }
        return XK_a + base;
    }
    if (keycode >= KEYCODE_0 && keycode <= KEYCODE_9)
    {
        int base = keycode - KEYCODE_0;
        return XK_0 + base;
    }
    switch (keycode)
    {
    case KEYCODE_COMMA: return XK_comma;
    case KEYCODE_PERIOD: return XK_period;
    case KEYCODE_ALT_LEFT: return XK_Alt_L;
    case KEYCODE_ALT_RIGHT: return XK_Alt_R;
    case KEYCODE_SHIFT_LEFT: return XK_Shift_L;
    case KEYCODE_SHIFT_RIGHT: return XK_Shift_R;
    case KEYCODE_TAB: return XK_Tab;
    case KEYCODE_SPACE: return XK_space;
    case KEYCODE_SYM: return -1;
    case KEYCODE_EXPLORER: return -1;
    case KEYCODE_ENVELOPE: return -1;
    case KEYCODE_ENTER: return XK_Return;
    case KEYCODE_DEL: return XK_BackSpace;
    case KEYCODE_GRAVE: return XK_grave;
    case KEYCODE_MINUS: return XK_minus;
    case KEYCODE_EQUALS: return XK_equal;
    case KEYCODE_LEFT_BRACKET: return XK_bracketleft;
    case KEYCODE_RIGHT_BRACKET: return XK_bracketright;
    case KEYCODE_BACKSLASH: return XK_backslash;
    case KEYCODE_SEMICOLON: return XK_semicolon;
    case KEYCODE_APOSTROPHE: return XK_apostrophe;
    case KEYCODE_SLASH: return XK_slash;
    case KEYCODE_AT: return XK_at;
    case KEYCODE_NUM: return XK_Num_Lock;
    case KEYCODE_HEADSETHOOK: return -1;
    case KEYCODE_FOCUS: return -1;
    case KEYCODE_PLUS: return XK_plus;
    case KEYCODE_MENU: return -1;
    case KEYCODE_NOTIFICATION: return -1;
    case KEYCODE_SEARCH: return -1;
    case KEYCODE_MEDIA_PLAY_PAUSE: return -1;
    case KEYCODE_MEDIA_STOP: return -1;
    case KEYCODE_MEDIA_NEXT: return -1;
    case KEYCODE_MEDIA_PREVIOUS: return -1;
    case KEYCODE_MEDIA_REWIND: return -1;
    case KEYCODE_MEDIA_FAST_FORWARD: return -1;
    case KEYCODE_MUTE: return -1;
    case KEYCODE_PAGE_UP: return XK_Page_Up;
    case KEYCODE_PAGE_DOWN: return XK_Page_Down;
    case KEYCODE_F1: return XK_F1;
    case KEYCODE_F2: return XK_F2;
    case KEYCODE_F3: return XK_F3;
    case KEYCODE_F4: return XK_F4;
    case KEYCODE_F5: return XK_F5;
    case KEYCODE_F6: return XK_F6;
    case KEYCODE_F7: return XK_F7;
    case KEYCODE_F8: return XK_F8;
    case KEYCODE_F9: return XK_F9;
    case KEYCODE_F10: return XK_F10;
    case KEYCODE_F11: return XK_F11;
    case KEYCODE_F12: return XK_F12;
    case KEYCODE_ESCAPE: return XK_Escape;
    case KEYCODE_CTRL_LEFT: return XK_Control_L;
    case KEYCODE_CTRL_RIGHT: return XK_Control_R;
    case KEYCODE_CAPS_LOCK: return XK_Caps_Lock;
    case KEYCODE_DPAD_UP: return XK_Up;
    case KEYCODE_DPAD_DOWN: return XK_Down;
    case KEYCODE_DPAD_LEFT: return XK_Left;
    case KEYCODE_DPAD_RIGHT: return XK_Right;
    case KEYCODE_FORWARD_DEL: return XK_Delete;
    }
    return -1;
}

