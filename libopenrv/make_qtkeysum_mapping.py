#!/usr/bin/python
# -*- coding: utf-8
#
# Copyright (C) 2018 Monument-Software GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


import os
import sys
import re

# Input file: Path to keysymdef.h from X11, on linux usually at /usr/include/X11/keysymdef.h
xKeysymFilePath = './keysymdef.h'

# Input file: Path to XF86keysym.h from X11, on linux usually at /usr/include/X11/XF86keysym.h
xf86keysymFilePath = './XF86keysym.h'

# Input file: Path to qnamespace.h from Qt5, on debian/ubuntu usually at /usr/include/qt5/QtCore/qnamespace.h
qtKeyFilePath = '/usr/include/qt5/QtCore/qnamespace.h'

outputFileName = 'public/libopenrv/orv_qtkey_to_xkeysym.h'


def myhex(v):
    return '0x%0.4x' % v

if not os.path.exists(qtKeyFilePath):
    qtKeyFilePath = os.path.join(os.path.expanduser('~'), 'Qt/5.5/clang_64/lib/QtCore.framework/Headers/qnamespace.h')
    if not os.path.exists(qtKeyFilePath):
        qtKeyFilePath = None

if not xKeysymFilePath or not os.path.exists(xKeysymFilePath):
    print("ERROR: Unable to find keysymdef.h")
    sys.exit(1)
if not xf86keysymFilePath or not os.path.exists(xf86keysymFilePath):
    print("ERROR: Unable to find XF86keysym.h")
    sys.exit(1)
if not qtKeyFilePath or not os.path.exists(qtKeyFilePath):
    print("ERROR: Unable to find qnamespace.h")
    sys.exit(1)

xkeysyms = {} # map mnemonic name (xxx in XK_xxx) to integer value.
xkeysymsUnicodePositionAndNames = {} # map mnemonic name (xxx in XK_xxx) to unicode position and name, if available
unicodePositionToXKeysymsName = {}   # unicode position (if available) to mnemonic name (xxx in XK_xxx)
xkeysymsvalues = set()
xkeysymsLowercaseToNameIfUnique = {}
xkeysymsNonUniqueLowercaseNames = set()
with open(xKeysymFilePath, 'r') as xkeysymfile:
    # NOTE: We use one of the regexps suggested in keysymdef.h, i.e. this should be dependable.
    xregexp = re.compile('^#define XK_([a-zA-Z_0-9]+)\s+0x([0-9a-f]+)\s*(\/\*\s*(.*)\s*\*\/)?\s*$')
    # keysymdef.h says:
    # "Where a keysym corresponds one-to-one to an ISO 10646 / Unicode
    # character, this is noted in a comment that provides both the U+xxxx
    # Unicode position, as well as the official Unicode name of the
    # character."
    unicodeRegexp = re.compile('^U\+([0-9A-F]{4,6}) (.*)\s*$')
    for line in xkeysymfile:
        m = xregexp.match(line)
        if m:
            name = m.group(1)    # Mnemonic name of the keysym without the XK_ prefix
            value = m.group(2)   # hex value without the 0x prefix
            value = int(value, 16)
            if value in xkeysymsvalues:
                # keysymdef.h says:
                # "Where several mnemonic names are defined for the same keysym in this
                # file, all but the first one listed should be considered deprecated."
                continue
            comment = m.group(4)
            unicodePosition = None
            unicodeName = None
            ignore = False
            if comment:
                m2 = unicodeRegexp.match(comment)
                if m2:
                    unicodePosition = int(m2.group(1), 16)
                    unicodeName = m2.group(2)
                else:
                    if comment.startswith('(') and comment.endswith(')'):
                        # NOTE: if comment is in brackets (i.e. starts with '(' and ends with ')'), the
                        #       keysym is considered a legacy keysym and should be considered
                        #       deprecated.
                        #       see keysymdef.h docs.
                        ignore = True
            if not ignore:
                xkeysyms[name] = value
                xkeysymsvalues.add(value)
                if unicodePosition is not None and unicodeName is not None:
                    xkeysymsUnicodePositionAndNames[name] = (unicodePosition, unicodeName)
                if unicodePosition is not None:
                    unicodePositionToXKeysymsName[unicodePosition] = name
                lowercaseName = name.lower()
                if lowercaseName not in xkeysymsNonUniqueLowercaseNames:
                    if lowercaseName in xkeysymsLowercaseToNameIfUnique:
                        del xkeysymsLowercaseToNameIfUnique[lowercaseName]
                        xkeysymsNonUniqueLowercaseNames.add(lowercaseName)
                    else:
                        xkeysymsLowercaseToNameIfUnique[lowercaseName] = name
        elif line.startswith('#define XK_'):
            raise Exception("Unhandled XK_ define in line %s" % (line.strip()))

# XF86keysym.h: "XFree86 vendor specific keysyms" (brightness up/down, hibernate, ..., i.e. special
#               keys)
xf86keysyms = {}
with open(xf86keysymFilePath, 'r') as xf86keysymFile:
    regexp = re.compile("^#define XF86XK_([a-zA-Z_0-9]+)\s+0x([0-9a-fA-F]+)\s*(\/\*\s*(.*)\s*\*\/)?\s*$")
    for line in xf86keysymFile:
        m = regexp.match(line)
        if m:
            name = m.group(1)
            value = int(m.group(2), 16)
            if value < 0x10080001 or value > 0x1008FFFF:
                # XF86keysym.h uses keysm range 0x10080001 - 0x1008FFFF
                # (NOTE: keysymdef.h uses 0x01000000 + unicode value for unicode chars.
                #        unicode has values up to 0x10ffff, so maximum value in keysymdef.h
                #        should be 0x0110ffff)
                raise Exception("Unexpected value in XF86keysym.h: %d" % value)
            comment = m.group(4)
            xf86keysyms[name] = value
        elif line.startswith("#define XF86XK_"):
            raise Exception("regexp missed define on line \"%s\"" % line.strip())

qtkeys = {}
with open(qtKeyFilePath, 'r') as qtKeyFile:
    qtkeyregexp = re.compile('^ *Key_([a-zA-Z_0-9]+)\s*=\s*0x([0-9a-fA-F]+)\s*,?\s*(\/\/.*)?$')
    for line in qtKeyFile:
        m = qtkeyregexp.match(line)
        if m:
            name = m.group(1)  # enum name without the Key_ prefix
            value = m.group(2) # hex value without the 0x prefix
            value = int(value, 16)
            qtkeys[name] = value

# Some special key handling
alternativeNames = {}
# Qt has "Alt" and "AltGr", X has "Alt_L" and "Alt_R"
alternativeNames['Alt'] = 'Alt_L'
alternativeNames['AltGr'] = 'Alt_R'
alternativeNames['PageUp'] = 'Prior' # Prior == Page_Up (which is marked as deprecated in keysymdef.h)
alternativeNames['PageDown'] = 'Next' # Next == Page_Down (which is marked as deprecated in keysymdef.h)
alternativeNames['ScrollLock'] = 'Scroll_Lock'
alternativeNames['CapsLock'] = 'Caps_Lock'
# X has Shift_L and Shift_R, Qt only Shift. We map to Shift_L.
alternativeNames['Shift'] = 'Shift_L'
# X has Meta_L and Meta_R, Qt only Meta. We map to Meta_L.
alternativeNames['Meta'] = 'Meta_L'
# X has Control_L and Control_R, Qt only Control. We map to Control_L.
alternativeNames['Control'] = 'Control_L'
alternativeNames['OpenUrl'] = 'OpenURL'
alternativeNames['NumLock'] = 'Num_Lock'
alternativeNames['MicMute'] = 'AudioMicMute'
alternativeNames['VolumeMute'] = 'AudioMute'
alternativeNames['VolumeUp'] = 'AudioRaiseVolume'
alternativeNames['VolumeDown'] = 'AudioLowerVolume'
alternativeNames['MediaStop'] = 'AudioStop'
alternativeNames['MediaRecord'] = 'AudioRecord'
alternativeNames['MediaNext'] = 'AudioNext'
alternativeNames['MediaPause'] = 'AudioPause'
alternativeNames['MediaPlay'] = 'AudioPlay'
alternativeNames['MediaPrevious'] = 'AudioPrev'
alternativeNames['LaunchMedia'] = 'AudioMedia'
alternativeNames['LaunchMail'] = 'Mail'
alternativeNames['KeyboardLightOnOff'] = 'KbdLightOnOff'
alternativeNames['KeyboardBrightnessDown'] = 'KbdBrightnessDown'
alternativeNames['KeyboardBrightnessUp'] = 'KbdBrightnessUp'
alternativeNames['Enter'] = 'KP_Enter'       # Qt::Key_Enter implies Qt::KeypadModifier
alternativeNames['Backtab'] = 'ISO_Left_Tab' # NOTE: names clearly differ but Qt maps the ISO_Left_Tab to Backtab, so this should be correct.

# X has separate keysyms for keypad keys.
# however Qt uses the "normal" keys and adds a KeypadModifier instead, so if we encounter such a
# key, we have to check for that modifier to return the correct keysym.
# All XK_KP_* keysyms are relevant.
keypadNames = {}
keypadNames['Space'] = 'KP_Space'
keypadNames['Tab'] = 'KP_Tab'
keypadNames['Enter'] = 'KP_Enter'
keypadNames['F1'] = 'KP_F1'
keypadNames['F2'] = 'KP_F2'
keypadNames['F3'] = 'KP_F3'
keypadNames['F4'] = 'KP_F4'
keypadNames['Home'] = 'KP_Home'
keypadNames['Left'] = 'KP_Left'
keypadNames['Up'] = 'KP_Up'
keypadNames['Right'] = 'KP_Right'
keypadNames['Down'] = 'KP_Down'
keypadNames['PageUp'] = 'KP_Prior'
keypadNames['PageDown'] = 'KP_Next'
keypadNames['End'] = 'KP_End'
keypadNames['Clear'] = 'KP_Begin'
keypadNames['Insert'] = 'KP_Insert'
keypadNames['Delete'] = 'KP_Delete'
keypadNames['Equal'] = 'KP_Equal'
keypadNames['Asterisk'] = 'KP_Multiply'
keypadNames['Plus'] = 'KP_Add'
keypadNames['Comma'] = 'KP_Separator'
keypadNames['Minus'] = 'KP_Subtract'
keypadNames['Period'] = 'KP_Decimal'
keypadNames['Slash'] = 'KP_Divide'
keypadNames['0'] = 'KP_0'
keypadNames['1'] = 'KP_1'
keypadNames['2'] = 'KP_2'
keypadNames['3'] = 'KP_3'
keypadNames['4'] = 'KP_4'
keypadNames['5'] = 'KP_5'
keypadNames['6'] = 'KP_6'
keypadNames['7'] = 'KP_7'
keypadNames['8'] = 'KP_8'
keypadNames['9'] = 'KP_9'

# keys not used by qt on x11 and/or no matching found in the keysymdef.h/XF86keysym.h
ignoreQtKeys = set()
ignoreQtKeys.add('unknown')
ignoreQtKeys.add('MicVolumeDown')
ignoreQtKeys.add('MicVolumeUp')
ignoreQtKeys.add('No')
ignoreQtKeys.add('Ooblique')
ignoreQtKeys.add('Play')
ignoreQtKeys.add('Printer')
ignoreQtKeys.add('QuoteLeft')
ignoreQtKeys.add('Settings')
ignoreQtKeys.add('SysReq')
ignoreQtKeys.add('ToggleCallHangup')
ignoreQtKeys.add('TrebleDown')
ignoreQtKeys.add('TrebleUp')
ignoreQtKeys.add('VoiceDial')
ignoreQtKeys.add('Yes')
ignoreQtKeys.add('Zoom')
ignoreQtKeys.add('MediaTogglePlayPause')
ignoreQtKeys.add('MediaLast')
ignoreQtKeys.add('Exit')
ignoreQtKeys.add('Flip')
ignoreQtKeys.add('Guide')
ignoreQtKeys.add('Hangup')
ignoreQtKeys.add('Henkan')
ignoreQtKeys.add('Direction_L')
ignoreQtKeys.add('Direction_R')
ignoreQtKeys.add('Info')
ignoreQtKeys.add('LastNumberRedial')
ignoreQtKeys.add('ChannelDown')
ignoreQtKeys.add('ChannelUp')
ignoreQtKeys.add('Context1')
ignoreQtKeys.add('Context2')
ignoreQtKeys.add('Context3')
ignoreQtKeys.add('Context4')
ignoreQtKeys.add('BassBoost')
ignoreQtKeys.add('BassDown')
ignoreQtKeys.add('BassUp')
ignoreQtKeys.add('Call')
ignoreQtKeys.add('Camera')
ignoreQtKeys.add('CameraFocus')
# For Launch0,...,LaunchF, Qt has weird behavior: Key_Launch0 is mapped to XF86XK_MyComputer,
# Key_Launch1 to XF86XK_Calculator, and Key_LaunchN for N>1 (up to LaunchH) to XF86XK_LaunchX with
# X:=N-2.
# Instead to follow this mapping, we simply ignore them all, they are not really relevant for our
# vnc use-case anyway.
ignoreQtKeys.add('Launch0')
ignoreQtKeys.add('Launch1')
ignoreQtKeys.add('Launch2')
ignoreQtKeys.add('Launch3')
ignoreQtKeys.add('Launch4')
ignoreQtKeys.add('Launch5')
ignoreQtKeys.add('Launch6')
ignoreQtKeys.add('Launch7')
ignoreQtKeys.add('Launch8')
ignoreQtKeys.add('Launch9')
ignoreQtKeys.add('LaunchA')
ignoreQtKeys.add('LaunchB')
ignoreQtKeys.add('LaunchC')
ignoreQtKeys.add('LaunchD')
ignoreQtKeys.add('LaunchE')
ignoreQtKeys.add('LaunchF')
ignoreQtKeys.add('LaunchG')
ignoreQtKeys.add('LaunchH')



#for xkeyname in sorted(xkeysyms.keys()):
#    if not xkeyname in qtkeys:
#        print xkeyname
with open(outputFileName, "w") as outputFile:
    outputFile.write('/* Auto-generated file from make_qtkeysym_mapping.py\n')
    outputFile.write('   All changes will be overwritten.\n\n')
    outputFile.write('   This file provides convenience functions to map a Qt::Key to a X11 keysym value\n')
    outputFile.write('   as used by the VNC protocol.\n\n')
    outputFile.write('   See also the keysymdef.h/XF86keysym.h files of the X server for the keysym values. */\n\n')
    outputFile.write('#ifndef OPENRV_QTKEY_TO_XKEYSYM_H\n')
    outputFile.write('#define OPENRV_QTKEY_TO_XKEYSYM_H\n')
    outputFile.write('\n')
    outputFile.write('#include <QtCore/qnamespace.h>\n')
    outputFile.write('#include <QtCore/qstring.h>\n')
    outputFile.write('\n')
    outputFile.write('inline int orv_qt_key_to_keysym(int qtKey, int qtModifiers, const QString& text)\n')
    outputFile.write('{\n')
    outputFile.write('  if (text.size() == 1) {\n')
    outputFile.write('    switch (text.unicode()[0].unicode()) {\n')
    for u in unicodePositionToXKeysymsName.keys():
        outputFile.write('      case ' + myhex(u) + ': return ' + myhex(xkeysyms[unicodePositionToXKeysymsName[u]]) + ';\n')
    outputFile.write('      default: break;')
    outputFile.write('    }\n')
    outputFile.write('  }\n')
    outputFile.write('  switch (qtKey) {\n')
    for qtkeyname in sorted(qtkeys.keys()):
        if qtkeyname in ignoreQtKeys:
            continue
        qtValue = qtkeys[qtkeyname]
        xkeysymsValue = None
        xkeysymsShiftValues = None
        if qtkeyname.lower() in xkeysymsLowercaseToNameIfUnique:
            xkeysymsValue = xkeysyms[xkeysymsLowercaseToNameIfUnique[qtkeyname.lower()]]
        elif qtkeyname in xkeysyms:
            if not qtkeyname.lower() in xkeysymsNonUniqueLowercaseNames:
                raise Exception("Internal error: qtkeyname %s not found with lowercase name in 'unqiue' dict, but also not found in non-unique set!" % qtkeyname)
            # The qt key name may map to multiple keysyms, depending e.g. on whether uppercase or
            # lowercase letter is requested.
            if qtkeyname.lower() in xkeysyms and qtkeyname.upper() in xkeysyms:
                xkeysymsValue = None
                xkeysymsShiftValues = [xkeysyms[qtkeyname.lower()], xkeysyms[qtkeyname.upper()]]
            elif qtkeyname.lower() in xkeysyms:
                xkeysymsValue = xkeysyms[qtkeyname.lower()]
            else:
                # fallback.
                # could happen if X keysyms name consists of multiple parts but not all lowercase.
                # (currently we have no such key)
                xkeysymsValue = xkeysyms[qtkeyname]
        elif qtkeyname in alternativeNames and alternativeNames[qtkeyname] in xkeysyms:
            # NOTE: atm we have no uppercase/lowercase variants in alternativeNames.
            xkeysymsValue = xkeysyms[alternativeNames[qtkeyname]]
        elif qtkeyname in xf86keysyms:
            xkeysymsValue = xf86keysyms[qtkeyname]
        elif qtkeyname in alternativeNames and alternativeNames[qtkeyname] in xf86keysyms:
            xkeysymsValue = xf86keysyms[alternativeNames[qtkeyname]]

        # NOTE: We assume that both, keysymdef.h and XF86keysym.h are fixed, i.e. the values
        #       of the keysyms will not change. I believe this is the case, so we can simply
        #       use the values directly here (no need to include the X headers, which we would
        #       also have to ship+install on systems without X).
        #       If they are not fixed, the VNC protocol has serious problems, because it
        #       mandates that the keysym values are transmitted.
        if xkeysymsShiftValues is not None:
            outputFile.write('    case Qt::Key_' + qtkeyname + ': ')
            outputFile.write('if (qtModifiers & Qt::ShiftModifier) { return ' + myhex(xkeysymsShiftValues[1]) + '; } else { return ' + myhex(xkeysymsShiftValues[0]) + '; }\n')
        elif xkeysymsValue is not None:
            if qtkeyname == 'Control' or qtkeyname == 'Meta':
                # special case on osx: Qt::Key_Control maps to the CommandKey and Key_Meta to ctrl.
                # however when controlling a remote system, it seems more logical to send ctrl as
                # ctrl instead. so we swap Control and Meta on osx.
                outputFile.write('#if defined(Q_OS_OSX)\n')
                if qtkeyname == 'Control':
                    outputFile.write('    case Qt::Key_Meta: return ' + myhex(xkeysymsValue) + ';\n')
                elif qtkeyname == 'Meta':
                    outputFile.write('    case Qt::Key_Control: return ' + myhex(xkeysymsValue) + ';\n')
                outputFile.write('#else\n')

            outputFile.write('    case Qt::Key_' + qtkeyname + ': ')

            # NOTE: the keypad keys should all be unique w.r.t. the shift modifer.
            if qtkeyname in keypadNames:
                xkeysymsValueKeypad = xkeysyms[keypadNames[qtkeyname]] # all keypadNames values should exist in xkeysyms
                outputFile.write('if (qtModifiers & Qt::KeypadModifier) { return ' +
                        myhex(xkeysymsValueKeypad) + '; } else { return ' + myhex(xkeysymsValue) + '; }\n')
            else:
                outputFile.write('return ' + myhex(xkeysymsValue) + ';\n')

            if qtkeyname == 'Control' or qtkeyname == 'Meta':
                outputFile.write('#endif\n')
        else:
            if qtkeyname.lower() in xkeysymsNonUniqueLowercaseNames:
                print "WARNING: Qt key not found in keysymdef.h, but lowercase name found in non-unique list: %s" % qtkeyname
            else:
                print "WARNING: Qt key not found in keysymdef.h: %s" % qtkeyname
    outputFile.write('    default: return 0;\n')
    outputFile.write('  }\n')
    outputFile.write('}\n')
    outputFile.write('#endif\n')


# TODO: special case on osx: swap meta and ctrl? (Qt::Key_Control is the command key, NOT Ctrl)

# TODO: special handling for Shift/Meta/Control/Alt, Qt has only one variant, native systems usually
# have a left and right variant. maybe add a native event listener that looks for ctrl etc. key
# events and tracks which (left or right) was pressed last - use that flag when the qt key event
# arrives. (using the native events completely is much more tricks, because we also have to figure
# out whether the event for a given window is also meant for the given child widget)

