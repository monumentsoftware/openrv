#ifndef OPENRV_ORV_VERSION_H
#define OPENRV_ORV_VERSION_H

#define LIBOPENRV_VERSION_MAJOR    ${ORV_VERSION_MAJOR}
#define LIBOPENRV_VERSION_MINOR    ${ORV_VERSION_MINOR}
#define LIBOPENRV_VERSION_PATCH    ${ORV_VERSION_PATCH}
/**
 * The library version as a single define.
 *
 * The version consists of @ref LIBOPENRV_VERSION_MAJOR, @ref LIBOPENRV_VERSION_MINOR and
 * @ref LIBOPENRV_VERSION_PATCH in hex notation, where each version part takes 8 bit. Consequently
 * the version numbers can be easily compared in code and are still human readable: version 3.2.1
 * would be 0x030201.
 **/
#define LIBOPENRV_VERSION          ${ORV_VERSION}
#define LIBOPENRV_VERSION_STRING   "${ORV_VERSION_STRING}"
#define LIBOPENRV_COPYRIGHT_STRING "${ORV_COPYRIGHT_STRING}"

#endif

