set(ORV_VERSION_MAJOR    0)
set(ORV_VERSION_MINOR    0)
set(ORV_VERSION_PATCH    1)
set(ORV_COPYRIGHT_STRING "Copyright (c) 2018 Monument-Software GmbH")

if (${ORV_VERSION_MAJOR} GREATER 255 OR ${ORV_VERSION_MINOR} GREATER 255 OR ${ORV_VERSION_PATCH} GREATER 255)
    message(FATAL_ERROR "Version numbers must not exceed 255")
endif ()
set(ORV_VERSION_STRING   "${ORV_VERSION_MAJOR}.${ORV_VERSION_MINOR}.${ORV_VERSION_PATCH}")

#
# Helper function for dec_to_hex
#
macro(dec_halfbyte_to_hex_char hex dec)
    if (${dec} LESS 10)
        set(${hex} ${dec})
    elseif (${dec} LESS 16)
        math(EXPR v "65 + ${dec} - 10") # 'A' + (dec-10)
        string(ASCII ${v} ${hex})
    endif ()
endmacro()
#
# Convert a single (unsigned) byte value in decimal representation (i.e. values 0..255) to
# hexdecimal representation without the "0x" prefix (i.e. output "00" .. "FF").
#
macro(dec_ubyte_to_hex hex dec)
    if (NOT ${dec} LESS 256)
        message(FATAL_ERROR "Unsupported input value '${dec}' for dec_ubyte_to_hex macro.")
    endif ()
    math(EXPR v1 "(${dec} >> 4) & 15")
    dec_halfbyte_to_hex_char(hex1 ${v1})
    math(EXPR v2 "${dec} & 15")
    dec_halfbyte_to_hex_char(hex2 ${v2})
    set(${hex} "${hex1}${hex2}")
endmacro()
#
# ORV_VERSION combines major/minor/patch into a single (hex) value.
#
dec_ubyte_to_hex(major_hex ${ORV_VERSION_MAJOR})
dec_ubyte_to_hex(minor_hex ${ORV_VERSION_MINOR})
dec_ubyte_to_hex(patch_hex ${ORV_VERSION_PATCH})
set(ORV_VERSION          0x${major_hex}${minor_hex}${patch_hex})

