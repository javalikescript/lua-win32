#include <lua.h>
#include <lauxlib.h>

//#define JLS_LUA_MOD_TRACE 1

#include "lua-compat/luamod.h"

#if LUA_VERSION_NUM < 503
#include "lua-compat/compat.h"
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

BOOL CALLBACK enum_windows_callback(HWND hwnd, LPARAM lParam) {
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

static HWND find_current_process_window(void) {
  EnumData ed;
  ed.processId = GetCurrentProcessId();
  ed.hwnd = NULL;
  trace("find_current_process_window() pid is %lu\n", ed.processId);
  if (!EnumWindows(enum_windows_callback, (LPARAM) &ed) && (GetLastError() == ERROR_SUCCESS)) {
    return ed.hwnd;
  }
  return NULL;
}

static void* lua_newbuffer(lua_State *l, size_t size) {
  void *p = lua_newuserdata(l, size);
  lua_pop(l, 1);
  return p;
}

#define LUA_WIN32_DEFAULT_CODE_PAGE CP_UTF8

static UINT codePage = LUA_WIN32_DEFAULT_CODE_PAGE;

static void decode_string(WCHAR *ws, DWORD n, const char *s) {
  MultiByteToWideChar(codePage, 0, s, -1, ws, n);
}

static WCHAR *new_decoded_string(lua_State *l, const char *s) {
  DWORD n;
  WCHAR *ws = NULL;
  if (s != NULL) {
    n = MultiByteToWideChar(codePage, 0, s, -1, 0, 0);
    ws = (WCHAR *)lua_newbuffer(l, sizeof(WCHAR) * n);
    if (ws != NULL) {
      decode_string(ws, n, s);
    }
  }
  return ws;
}

static void encode_string(char *s, DWORD n, const WCHAR *ws) {
  WideCharToMultiByte(codePage, 0, ws, -1, s, n, NULL, NULL);
}

static char *new_encoded_string(lua_State *l, WCHAR *ws) {
  int n;
  char *s = NULL;
  if (ws != NULL) {
    n = WideCharToMultiByte(codePage, 0, ws, -1, NULL, 0, NULL, NULL);
    s = (char *)lua_newbuffer(l, n);
    if (s != NULL) {
      encode_string(s, n, ws);
    }
  }
  return s;
}

static void push_encoded_string(lua_State *l, WCHAR *ws) {
  if (ws != NULL) {
    char *s = new_encoded_string(l, ws);
    if (s != NULL) {
      lua_pushstring(l, s);
      return;
    }
  }
  lua_pushnil(l);
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

static int win32_OutputDebugString(lua_State *l) {
  OutputDebugStringW(new_decoded_string(l, lua_tostring(l, 1)));
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
  "none", "check", "process", "process-check", NULL
};

static int win32_SetWindowOwner(lua_State *l) {
  int opt = luaL_checkoption(l, 1, "process-check", win32_window_owner_names);
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
    hwndOwner = find_current_process_window();
    break;
  case 3:
    if ((hwndOwner == NULL) || !IsWindow(hwndOwner)) {
      hwndOwner = find_current_process_window();
    }
    break;
  }
  trace("win32_SetWindowOwner() %p\n", hwndOwner);
  lua_pushboolean(l, hwndOwner != NULL);
  return 1;
}

static int win32_ShellExecute(lua_State *l) {
	int showCmd = SW_SHOWNORMAL;
  WCHAR operation[32];
  decode_string(operation, 32, luaL_optstring(l, 1, NULL));
  WCHAR *file = new_decoded_string(l, luaL_optstring(l, 2, NULL));
  WCHAR *parameters = new_decoded_string(l, luaL_optstring(l, 3, NULL));
  WCHAR *directory = new_decoded_string(l, luaL_optstring(l, 4, NULL));
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
  WCHAR *text = new_decoded_string(l, luaL_optstring(l, 1, NULL));
  WCHAR *caption = new_decoded_string(l, luaL_optstring(l, 2, NULL));
  unsigned int type = luaL_optinteger(l, 3, MB_OK);
  trace("win32_MessageBox()\n");
  int result = MessageBoxW(hwndOwner, text, caption, type);
	lua_pushinteger(l, result);
  return 1;
}

#define FOLDERNAME_MAX_SIZE (MAX_PATH * 2)
#define FILENAME_MAX_SIZE (MAX_PATH / 2)
#define OPENFILES_MAX_COUNT 24
#define OPENFILES_MAX_SIZE (FOLDERNAME_MAX_SIZE + FILENAME_MAX_SIZE * OPENFILES_MAX_COUNT)
#define DEFAULT_FLAGS (OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_EXPLORER)

static int GetFilename(lua_State *l, const char *filename, int isSave, int flags) {
  BOOL done;
	OPENFILENAMEW ofn;
	WCHAR fb[OPENFILES_MAX_SIZE];
  if (filename == NULL) {
    fb[0] = 0;
  } else {
    decode_string(fb, OPENFILES_MAX_SIZE, filename);
  }
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwndOwner;
  ofn.hInstance = GetModuleHandle(NULL);
  ofn.lpstrFilter = NULL; // A buffer containing pairs of null-terminated filter strings, terminated by two NULL characters.
  ofn.lpstrCustomFilter = NULL;
  ofn.nFilterIndex = 0;
  ofn.lpstrFile = fb;
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
        count++;
        trace("file[%d]: \"%ls\"\n", count, p);
        push_encoded_string(l, p);
        p += len + 1;
      }
      return count;
    }
    trace("file: \"%ls\"\n", p);
    push_encoded_string(l, ofn.lpstrFile);
    return 1;
  }
  return 0;
}

static int win32_GetOpenFileName(lua_State *l) {
  // OFN_CREATEPROMPT OFN_FILEMUSTEXIST
  int flagIndex = 1;
  int flags = DEFAULT_FLAGS;
  const char *filename = NULL;
  if (!lua_isboolean(l, 1)) {
    filename = luaL_optstring(l, 1, NULL);
    flagIndex = 2;
  }
  if (lua_toboolean(l, flagIndex)) {
    flags |= OFN_ALLOWMULTISELECT;
  }
  return GetFilename(l, filename, 0, flags);
}

static int win32_GetSaveFileName(lua_State *l) {
  // OFN_OVERWRITEPROMPT
  return GetFilename(l, luaL_optstring(l, 1, NULL), 1, DEFAULT_FLAGS);
}

static int win32_WaitProcessId(lua_State *l) {
  DWORD exitCode = 0;
  DWORD waitResult = 0;
  BOOL exitCodeResult = FALSE;
  DWORD pid = (DWORD) luaL_checkinteger(l, 1);
  int timoutMs = luaL_optinteger(l, 2, 0);
  int getExitCode = lua_toboolean(l, 3);
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid);
  if (hProcess != NULL) {
    waitResult = WaitForSingleObject(hProcess, timoutMs);
    if (getExitCode && (waitResult == WAIT_OBJECT_0)) {
      exitCodeResult = GetExitCodeProcess(hProcess, &exitCode);
    }
    CloseHandle(hProcess);
  }
  lua_pushinteger(l, waitResult);
  if (exitCodeResult) {
    lua_pushinteger(l, exitCode);
    return 2;
  }
  return 1;
}

static int win32_TerminateProcessId(lua_State *l) {
  BOOL result = FALSE;
  DWORD pid = (DWORD) luaL_checkinteger(l, 1);
  int exitCode = luaL_optinteger(l, 2, 0);
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, pid);
  if (hProcess != NULL) {
    result = TerminateProcess(hProcess, exitCode);
    CloseHandle(hProcess);
  }
  lua_pushboolean(l, result);
  return 1;
}

static int win32_GetExitCodeProcess(lua_State *l) {
  BOOL result = FALSE;
  DWORD pid = (DWORD) luaL_checkinteger(l, 1);
  DWORD exitCode = 0;
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (hProcess != NULL) {
    result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);
    if (result) {
      lua_pushinteger(l, exitCode);
      return 1;
    }
  }
  lua_pushnil(l);
  return 1;
}

static int win32_GetCurrentProcessId(lua_State *l) {
	lua_pushinteger(l, GetCurrentProcessId());
  return 1;
}

#define set_int_field(__LUA_STATE, __FIELD_NAME) \
  lua_pushinteger(__LUA_STATE, __FIELD_NAME); \
  lua_setfield(__LUA_STATE, -2, #__FIELD_NAME)

LUALIB_API int luaopen_win32(lua_State *l) {
  trace("luaopen_win32()\n");
  luaL_Reg reg[] = {
    { "GetLastError", win32_GetLastError },
    { "GetConsoleOutputCodePage", win32_GetConsoleOutputCodePage },
    { "GetCodePage", win32_GetCodePage },
    { "SetCodePage", win32_SetCodePage },
    { "OutputDebugString", win32_OutputDebugString },
    { "GetMessageFromSystem", win32_GetMessageFromSystem },
    { "GetCommandLine", win32_GetCommandLine },
    { "GetCommandLineArguments", win32_GetCommandLineArguments },
    { "SetWindowOwner", win32_SetWindowOwner },
    { "ShellExecute", win32_ShellExecute },
    { "MessageBox", win32_MessageBox },
    { "GetOpenFileName", win32_GetOpenFileName },
    { "GetSaveFileName", win32_GetSaveFileName },
    { "WaitProcessId", win32_WaitProcessId },
    { "GetExitCodeProcess", win32_GetExitCodeProcess },
    { "TerminateProcessId", win32_TerminateProcessId },
    { "GetCurrentProcessId", win32_GetCurrentProcessId },
    { NULL, NULL }
  };
  lua_newtable(l);
  luaL_setfuncs(l, reg, 0);

  lua_newtable(l);
  // wait
  set_int_field(l, WAIT_ABANDONED);
  set_int_field(l, WAIT_OBJECT_0);
  set_int_field(l, WAIT_TIMEOUT);
  set_int_field(l, WAIT_FAILED);
  // exit code
  set_int_field(l, STILL_ACTIVE);
  lua_setfield(l, -2, "constants");

  lua_pushliteral(l, "Lua win32");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.2");
  lua_setfield(l, -2, "_VERSION");
  trace("luaopen_win32() done\n");
  return 1;
}
