The Lua win32 module exposes miscellaneous functions from the Windows API.
If you are looking for other Windows APIs then please have a look into the [winapi](https://github.com/stevedonovan/winapi) Lua module.

```lua
local win32 = require('win32')
win32.SetCodePage('utf-8') -- Sets the module encoding to UTF-8 which is the default
print(win32.GetCommandLineArguments()) -- Returns the current process arguments as strings
```

Lua win32 is covered by the MIT license.

The functions are the following:
* _GetLastError_
Returns the last error as an integer.
* _GetConsoleOutputCodePage_
Returns the console output code page as an integer.
* _GetCodePage_
Returns the win32 module code page as an integer.
The module code page is used to decode and encode strings.
* _SetCodePage_
Sets the win32 module code page, default is UTF-8.
The code page could be passed as an integer or one of the followed strings: "default", "console", "utf-8", "ansi", "oem", "symbol"
* _GetMessageFromSystem_
Returns the message associated to the specified error or to the last error
* _GetCommandLine_
Returns the command line as a string
* _GetCommandLineArguments_
Returns the command line arguments as a strings
* _SetWindowOwner_
Sets the window used in the _ShellExecute_, _MessageBox_, _GetOpenFileName_ and _GetSaveFileName_ functions
* _ShellExecute_
Performs an operation on the specified file, such ad "open" or "edit".
* _MessageBox_
Shows a message box with the specified text, title and type, default to OK. Returns the message box result. This method will block.
* _GetOpenFileName_
Shows the file selection dialog and returns the selected file name as string.
If multiple selection is enabled then returns the selected path followed by the file names as strings. This method will block.
* _GetSaveFileName_
Shows the file selection dialog and returns the selected file name as string. This method will block.
* _WaitProcessId_
Waits for a process identifier to terminate
* _GetExitCodeProcess_
Returns a process identifier exit code
* _TerminateProcessId_
Terminates a process identifier
* _GetCurrentProcessId_
Returns the current process identifier

The field _constants_ contains constant values.
