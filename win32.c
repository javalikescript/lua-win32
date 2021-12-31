#include <lua.h>
#include <lauxlib.h>

//#define JLS_LUA_MOD_TRACE 1

#ifdef JLS_LUA_MOD_TRACE
#include <stdio.h>
#define trace(...) printf(__VA_ARGS__)
#else
#define trace(...) ((void)0)
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>

static HWND hwndOwner = NULL;

typedef struct {
  DWORD processId;
  HWND hwnd;
} EnumData;

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
  EnumData *ed = (EnumData*)lParam;
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (ed->processId == processId) {
    ed->hwnd = hwnd;
    SetLastError(ERROR_SUCCESS);
    return FALSE;
  }
  return TRUE;
}

static HWND FindCurrentProcessWindow(void) {
  EnumData ed;
  ed.processId = GetCurrentProcessId();
  ed.hwnd = NULL;
  if (!EnumWindows(EnumProc, (LPARAM) &ed) && (GetLastError() == ERROR_SUCCESS)) {
    return ed.hwnd;
  }
  return NULL;
}

#define LUA_WIN32_DEFAULT_CODE_PAGE CP_UTF8

static UINT codePage = LUA_WIN32_DEFAULT_CODE_PAGE;

static WCHAR *decode_string(const char *s) {
  DWORD size;
  WCHAR *ws = NULL;
  if (s != NULL) {
    size = MultiByteToWideChar(codePage, 0, s, -1, 0, 0);
    ws = (WCHAR *)GlobalAlloc(GMEM_FIXED, sizeof(WCHAR) * size); // We may let Lua manage the memory
    if (ws != NULL) {
      MultiByteToWideChar(codePage, 0, s, -1, ws, size);
    }
  }
  return ws;
}

static char *encode_string(WCHAR *ws) {
  int n;
  char *s = NULL;
  if (ws != NULL) {
    n = WideCharToMultiByte(codePage, 0, ws, -1, NULL, 0, NULL, NULL);
    s = (char *)GlobalAlloc(GMEM_FIXED, n);
    if (s != NULL) {
      WideCharToMultiByte(codePage, 0, ws, -1, s, n, NULL, NULL);
    }
  }
  return s;
}

static int push_encoded_string(lua_State *l, WCHAR *ws) {
  if (ws != NULL) {
    char *s = encode_string(ws);
    if (s != NULL) {
      lua_pushstring(l, s);
      GlobalFree(s);
      return 0;
    }
  }
  lua_pushnil(l);
  return 1;
}

static const char *const win32_code_page_names[] = {
  "default", "console", "utf-8", "ansi", "oem", "symbol", NULL
};

static const UINT win32_code_pages[] = {
  LUA_WIN32_DEFAULT_CODE_PAGE, 0, CP_UTF8, CP_ACP, CP_OEMCP, CP_SYMBOL
};

static UINT get_code_page_arg(lua_State *l, int idx, UINT def) {
  int opt;
  UINT cp = def;
	if (lua_isinteger(l, idx)) {
  	cp = (UINT) lua_tointeger(l, idx);
  } else if (lua_isstring(l, idx)) {
  	opt = luaL_checkoption(l, idx, NULL, win32_code_page_names);
    if (opt == 1) {
      cp = GetConsoleOutputCP();
    } else {
      cp = win32_code_pages[opt];
    }
  }
  return cp;
}

static int win32_GetConsoleOutputCodePage(lua_State *l) {
	lua_pushinteger(l, GetConsoleOutputCP());
  return 1;
}

static int win32_GetCodePage(lua_State *l) {
	lua_pushinteger(l, codePage);
  return 1;
}

static int win32_SetCodePage(lua_State *l) {
  codePage = get_code_page_arg(l, 1, LUA_WIN32_DEFAULT_CODE_PAGE);
  return 0;
}

static int win32_GetLastError(lua_State *l) {
	lua_pushinteger(l, GetLastError());
  return 1;
}

static int win32_GetMessageFromSystem(lua_State *l) {
	DWORD err = 0;
	DWORD ret = 0;
  WCHAR buffer[MAX_PATH + 2];
	if (lua_isinteger(l, 1)) {
		err = (DWORD) lua_tointeger(l, 1);
  } else {
  	err = GetLastError();
  }
	ret = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, buffer, MAX_PATH, NULL);
  if (ret == 0) {
    lua_pushnil(l);
  } else {
    push_encoded_string(l, buffer);
	}
  return 1;
}

static int win32_GetCommandLine(lua_State *l) {
  push_encoded_string(l, GetCommandLineW());
  return 1;
}

static int win32_GetCommandLineArguments(lua_State *l) {
  LPWSTR *szArglist;
  int nArgs;
  int i;
  szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
  if (szArglist == NULL) {
    return 0;
  }
  for (i = 0; i < nArgs; i++) {
    push_encoded_string(l, szArglist[i]);
  }
  LocalFree(szArglist);
  return nArgs;
}

static const char *const win32_window_owner_names[] = {
  "none", "check", "process", NULL
};

static int win32_SetWindowOwner(lua_State *l) {
  int opt = luaL_checkoption(l, 1, "process", win32_window_owner_names);
  switch(opt) {
  case 0:
    hwndOwner = NULL;
    break;
  case 1:
    if ((hwndOwner != NULL) && !IsWindow(hwndOwner)) {
      hwndOwner = NULL;
    }
    break;
  case 2:
    hwndOwner = FindCurrentProcessWindow();
    break;
  }
  trace("win32_SetWindowOwner() %p\n", hwndOwner);
  lua_pushboolean(l, hwndOwner != NULL);
  return 1;
}

static int win32_ShellExecute(lua_State *l) {
	int showCmd = SW_SHOWNORMAL;
  WCHAR *operation = decode_string(luaL_optstring(l, 1, NULL));
  WCHAR *file = decode_string(luaL_optstring(l, 2, NULL));
  WCHAR *parameters = decode_string(luaL_optstring(l, 3, NULL));
  WCHAR *directory = decode_string(luaL_optstring(l, 4, NULL));
  int result = (int) (INT_PTR) ShellExecuteW(hwndOwner, operation, file, parameters, directory, showCmd);
  if (result <= 32) {
    lua_pushnil(l);
  	lua_pushinteger(l, result);
    return 2;
  }
  lua_pushboolean(l, TRUE);
  return 1;
}

static int win32_MessageBox(lua_State *l) {
  WCHAR *text = decode_string(luaL_optstring(l, 1, NULL));
  WCHAR *caption = decode_string(luaL_optstring(l, 2, NULL));
  unsigned int type = luaL_optinteger(l, 3, MB_OK);
  trace("win32_MessageBox()\n");
  int result = MessageBoxW(hwndOwner, text, caption, type);
	lua_pushinteger(l, result);
  return 1;
}

#define FOLDERNAME_MAX_SIZE 512
#define FILENAME_MAX_SIZE 64
#define OPENFILES_MAX_COUNT 24
#define OPENFILES_MAX_SIZE (FOLDERNAME_MAX_SIZE + FILENAME_MAX_SIZE * OPENFILES_MAX_COUNT)
#define DEFAULT_FLAGS (OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_EXPLORER)

static int get_filename(lua_State *l, int isSave, int flags) {
  BOOL done;
	OPENFILENAMEW ofn;
	WCHAR filename[OPENFILES_MAX_SIZE];
  filename[0] = 0;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwndOwner;
  ofn.hInstance = GetModuleHandle(NULL);
  ofn.lpstrFilter = NULL;
  ofn.lpstrCustomFilter = NULL;
  ofn.nFilterIndex = 0;
  ofn.lpstrFile = filename;
  ofn.nMaxFile = OPENFILES_MAX_SIZE;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = NULL;
  ofn.Flags = flags;
  if (isSave) {
    done = GetSaveFileNameW(&ofn);
  } else {
    done = GetOpenFileNameW(&ofn);
  }
  if (done) {
    if ((ofn.Flags & OFN_ALLOWMULTISELECT) != 0) {
      int count = 0;
      WCHAR *p;
      for (p = ofn.lpstrFile;;) {
        int len = wcslen(p);
        if (len == 0) {
          break;
        }
        trace("file: \"%ls\"\n", p);
        push_encoded_string(l, p);
        count++;
        p += len + 1;
      }
      return count;
    }
    push_encoded_string(l, ofn.lpstrFile);
    return 1;
  }
  return 0;
}

static int win32_GetOpenFileName(lua_State *l) {
  // OFN_CREATEPROMPT OFN_FILEMUSTEXIST
  int flags = DEFAULT_FLAGS;
  if (lua_toboolean(l, 1)) {
    flags |= OFN_ALLOWMULTISELECT;
  }
  return get_filename(l, 0, flags);
}

static int win32_GetSaveFileName(lua_State *l) {
  // OFN_OVERWRITEPROMPT
  return get_filename(l, 1, DEFAULT_FLAGS);
}

LUALIB_API int luaopen_win32(lua_State *l) {
  trace("luaopen_win32()\n");
  luaL_Reg reg[] = {
    { "GetLastError", win32_GetLastError },
    { "GetConsoleOutputCodePage", win32_GetConsoleOutputCodePage },
    { "GetCodePage", win32_GetCodePage },
    { "SetCodePage", win32_SetCodePage },
    { "GetMessageFromSystem", win32_GetMessageFromSystem },
    { "GetCommandLine", win32_GetCommandLine },
    { "GetCommandLineArguments", win32_GetCommandLineArguments },
    { "SetWindowOwner", win32_SetWindowOwner },
    { "ShellExecute", win32_ShellExecute },
    { "MessageBox", win32_MessageBox },
    { "GetOpenFileName", win32_GetOpenFileName },
    { "GetSaveFileName", win32_GetSaveFileName },
    { NULL, NULL }
  };
  lua_newtable(l);
  luaL_setfuncs(l, reg, 0);
  lua_pushliteral(l, "Lua win32");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.1");
  lua_setfield(l, -2, "_VERSION");
  trace("luaopen_win32() done\n");
  return 1;
}
