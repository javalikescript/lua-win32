The Lua win32 module exposes functions from the Windows API.

```lua
local win32 = require('win32')
win32.SetCodePage('utf-8') -- Sets the encoding to UTF-8
local arguments = win32.GetCommandLineArguments() -- Returns an array table containing the current process arguments
```

Lua win32 is covered by the MIT license.
