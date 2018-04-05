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


with open('../libopenrv/keysymdef.h') as f:
    lines = f.readlines()

with open('../libopenrv/key_xkeygen.h', 'wb') as f:
    f.write('typedef struct SXKeyDefT\n{\n\tconst char* mName;\n\tint mXCode; // xkey code\n\tint mUCode; // unicode value\n} SXKeyDef;\n');
    f.write('const SXKeyDef xkey_defs[] = \n{\n')
    first = True
    for line in lines:
        if line.startswith('#define XK_'):
            tokens = line.split()
            tokens = filter(None, tokens)
            if len(tokens) > 4 and tokens[4].startswith('U+'):
                if not first:
                    f.write(',\n')
                first = False
                u = tokens[4][2:]
                f.write('\t{ "%s", %s, 0x%s }' % (tokens[1], tokens[2], u))

    f.write('\n};')

