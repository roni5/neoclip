/*
 * neoclip - Neovim clipboard provider
 * Last Change:  2021 May 16
 * License:      https://unlicense.org
 * URL:          https://github.com/matveyt/neoclip
 */


#include "neoclip.h"
#include <stdint.h>


// convert v/V/^V to MCHAR/MLINE/MBLOCK
int neo_type(int ch)
{
    switch (ch) {
    case 'c':
    case 'v':
        return 0;   // MCHAR
    case 'l':
    case 'V':
        return 1;   // MLINE
    case 'b':
    case '\026':
        return 2;   // MBLOCK
    default:
        return 255; // MAUTO
    }
}


// split UTF-8 string into lines (LF or CRLF) and save in table [lines, regtype]
// chop invalid data, e.g. trailing zero in Windows Clipboard
void neo_split(lua_State* L, int idx, const void* data, size_t cb, int type)
{
    // validate input
    luaL_checktype(L, idx, LUA_TTABLE);
    if (data == NULL || cb < 1)
        return;

    // pb points to start of line
    const uint8_t* pb = data;
    // off + rest = size of remaining text
    size_t off = 0, rest = cb;
    // i is Lua table index (one-based)
    int i = 1;
    // state: -1 after CR; 0 normal; 1, 2, 3 skip continuation octets
    int state = 0;

    // lines table
    lua_newtable(L);

    do {
        int c = pb[off];        // get next octet

        if (state > 0) {        // skip continuation octet(s)
            if (c < 0x80 || c >= 0xc0)
                break;          // non-continuation octet
            --state;
        } else if (c == 0) {    // NUL
            break;
        } else if (c == 10) {   // LF or CRLF
            // push current line
            lua_pushlstring(L, (const char*)pb, off - (state < 0));
            lua_rawseti(L, -2, i++);
            // adjust pb and off
            pb += off + 1;
            off = state = 0;
            continue;           // don't increment off
        } else if (c == 13) {   // have CR
            state = -1;
        } else if (c < 0x80) {  // 7 bits code
            state = 0;
        } else if (c < 0xc0) {  // unexpected continuation octet
            break;
        } else if (c < 0xe0) {  // 11 bits code
            state = 1;
        } else if (c < 0xf0) {  // 16 bits code
            state = 2;
        } else if (c < 0xf8) {  // 21 bits code
            state = 3;
        } else  // bad octet
            break;

        ++off;
    } while (--rest);

    // push last string w/o invalid rest
    lua_pushlstring(L, (const char*)pb, off/* + rest*/);
    lua_rawseti(L, -2, i);

    // save result
    lua_rawseti(L, idx, 1);
    lua_pushlstring(L, type == 0 ? "v" : type == 1 ? "V" : type == 2 ? "\026" :
        off ? "v" : "V" , sizeof(char));
    lua_rawseti(L, idx, 2);
}


// table concatenation (numeric indices only)
// return string on Lua stack
void neo_join(lua_State* L, int idx, const char* sep)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    int n = lua_objlen(L, idx);
    if (n > 0) {
        for (int i = 1; i < n; ++i) {
            lua_rawgeti(L, idx, i);
            luaL_addvalue(&b);
            luaL_addstring(&b, sep);
        }
        lua_rawgeti(L, idx, n);
        luaL_addvalue(&b);
    }

    luaL_pushresult(&b);
}
