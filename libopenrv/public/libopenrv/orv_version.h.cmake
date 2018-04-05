/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

