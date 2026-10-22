## list the functions of ghidra-mcp

The `ghidra-mcp` server exposes 8 tools right now:

1. **list_instances**: Lists the Ghidra instances it can find, with each one's project name, PID, open programs, and socket path or TCP URL. It also shows which instance is connected.
2. **connect_instance**: Switches the bridge to a Ghidra instance by project name or substring. After connecting, it registers that instance's analysis tools.
3. **import_file**: Imports a binary from disk into the current Ghidra project and opens it in CodeBrowser. Takes an absolute file path, plus optional destination folder, language and compiler spec (for raw firmware), and an auto-analysis flag (on by default).
4. **list_tool_groups**: Shows each tool category with its tool count, whether it's loaded, and its tool names.
5. **load_tool_group**: Loads all tools in a category (e.g. "function", "datatype") or everything with "all".
6. **unload_tool_group**: Unloads a category. Default groups can't be unloaded.
7. **search_tools**: Searches the full tool catalog by keyword, including groups that aren't loaded, and reports the exact load command needed for any that aren't callable yet.
8. **check_tools**: Checks whether specific named tools are callable, not loaded, or not found.

The bulk of the reverse-engineering functionality (function analysis, renaming, comments, cross-references, data types, and so on) lives in the analysis tool groups, which only appear after you connect to a Ghidra instance. I can't see that full list until a connection is made. If you connect to an instance and then run list_tool_groups, I can give you the complete breakdown.


## Use gridra MCP (it has to be exactly ghidra, if you can't use it just say so and we will try to figure ot what went wrong) to analyze this file "C:\tools\ghidra_12.1.4_PUBLIC\WinAPI_pa.exe". The file has a critical error that shuts down the program. It happens only sometimes. The program has pretty simple functionality, but sometimes it closes on it's own. if you find the problem tell me what it is and where it is in HEX. Don't fix the problem off the bat, first explain it to me. if you want me to give you access to the file ask for it. i need you to help me fix the HEX of the .exe

I couldn't analyze the file. The ghidra-mcp bridge has no running Ghidra instance to talk to, so I haven't looked at `WinAPI_pa.exe` yet and have no findings to report.

Here's what I checked:
- `list_instances` returned no running Ghidra instances.
- `import_file` failed with "No Ghidra instance connected."
- `connect_instance` tried the project name "WinAPI" and failed. The bridge found no socket instance and couldn't reach the TCP fallback at `http://127.0.0.1:8089`. The connection was refused, which usually means nothing is listening there.

To get this working, Ghidra needs to be open with the MCP plugin active:
1. Start Ghidra 12.1.4 from `C:\tools\ghidra_12.1.4_PUBLIC`.
2. Open or create a project. The project name needs to match what you connect to.
3. Open CodeBrowser and confirm the GhidraMCP plugin/server is enabled and listening on port 8089. Check the File > Configure or the plugin settings if you're not sure.
4. Let me know when it's running, and I'll retry `list_instances` and `connect_instance`, then import the file.

I also can't read `C:\tools\...` from here, since my container has no access to your Windows drive. If the bridge doesn't come up, you can upload the `.exe` to this chat and I can work from the binary directly, though that path is less thorough than Ghidra. I'll explain the cause before changing any bytes, as you asked.

## now the ghidra is running, do the analysis

Ghidra is up and the bridge sees the project, but no program is open yet. I'll load the tool groups and then look for the binary.

The program is open with 880 functions detected. Next I'll look at the entry point and the strings to see what the program does.

The import list has several APIs that a simple program wouldn't normally need: thread suspension and context access (`SuspendThread`, `GetThreadContext`), process memory reads (`ReadProcessMemory`), memory protection changes (`VirtualProtect`), and a debugger check (`IsDebuggerPresent`). Next I'll look at the strings and the main code path.

The strings show that this program embeds **mhook** (a function-hooking library) and uses it to hook `MessageBoxW`. The hook process suspends threads, rewrites code bytes, and builds trampolines. That's a likely place for an intermittent crash. I'll decompile the entry point and the main hook code next.

The user code is hidden behind the CRT startup, so I'll trace from the "Hook successfull" string to the function that uses it.

Two custom-named functions stand out: `DebuggerProbe` and `DebuggerRuntime`. I'll decompile them along with `invoke_main`, which calls the user's `main`.

`DebuggerProbe` and `DebuggerRuntime` are standard Visual Studio runtime stubs, so they're not the bug. The real program code is at `FUN_14002acb0`, which `invoke_main` calls through a thunk. I'll decompile that next.

`main` installs a hook (`FUN_14002ad60`) and then loops forever, calling `FUN_14002a9c0` every 10 ms. Next I'll examine the hook installer and the loop body.

The hook path is `FUN_14002ad60` → `FUN_14002ade0`, which is the mhook core that suspends threads and patches code. I'll decompile that core next, along with the `MessageBoxW` replacement at `LAB_1400238c5`.

The patch stage is `FUN_1400305d0`, and the thread-suspension stage is `FUN_14002fcd0`. I'll decompile both.

The overlap check `FUN_14002fad0` is correct as written. Next I'll check the thread callback `FUN_14002e280` (which inspects thread context) and the prologue analysis in `FUN_14002f840`.

`FUN_14002eb70` decodes instructions to cover the first 5 bytes, and `FUN_14002f4a0` builds the trampoline. The bug could be in the replacement function or in the data that points to the hook target, so I'll read the hook data and the replacement code.

The replacement path is a two-jump stub: `1400238c5` jumps to `14002a2c0`, which is the hook body, and `1400238ca` jumps to `140029cf3`, which is probably the original-function path. I'll decompile both.

I found the likely cause. In the hook body `FUN_14002a2c0`, a random value is generated and then checked. When the value is 2, the code calls `abort()`, which kills the process. The value comes from `rand() % 20 + 1`, so each run has about a 1-in-20 chance of crashing. That matches "only sometimes." Before I write it up, I need the exact bytes and file offset of the `abort` branch.

The abort branch is at `14002A37E`–`14002A38A`. Next I need the section layout so I can convert these virtual addresses into file offsets for the hex edit.

I found the crash. The program doesn't have a memory bug. The hooked `MessageBoxW` replacement calls `abort()` when a random number comes up as 2. I haven't changed the file or Ghidra's project.

**What the code does**

The hook body is at `FUN_14002a2c0`. It generates a random number:

```c
local_1e4 = rand() % 0x14 + 1;   // range 1..20
if (local_1e4 == 1) local_1e4 = 100;
else if (local_1e4 == 2) abort(); // <- the crash
```

Each run has a 1-in-20 chance (5%) of landing on 2 and calling `abort()`. `abort()` terminates the process immediately, with no error message, so it looks like the program closed on its own. The 1→100 mapping right next to it looks like an odd special case that's worth noting, but it isn't the crash.

The hook runs whenever the program shows its message box, which the main loop triggers repeatedly. So the 5% roll happens on every pass, and the program eventually dies.

**Where it is**

Ghidra's virtual addresses and the file offsets for the `.exe` (the `.text` section has VA `0x23000` and raw offset `0x400`, so file offset = RVA − `0x23000` + `0x400`):

| Purpose | Virtual address | File offset | Bytes |
|---|---|---|---|
| `cmp [rbp+0x24], 2` | `0x14002A37E` | `0x777E` | `83 7D 24 02` |
| `jnz` skipping the abort | `0x14002A382` | `0x7782` | `75 07` |
| `call [abort]` | `0x14002A384` | `0x7784` | `FF 15 D6 12 0A 00` |

To check the offsets in a hex editor before editing, search for this 12-byte pattern, which should appear only once:
`83 7D 24 02 75 07 FF 15 D6 12 0A 00`

**Proposed fix (not applied yet)**

Change the single byte at file offset `0x7782` from `75` (JNZ) to `EB` (JMP). The jump still goes to the same place, so the `abort()` call is never reached and the file size and every other address stay the same. The result should read `83 7D 24 02 EB 07 FF 15 D6 12 0A 00`.

I'd suggest patching a copy of the `.exe` so the original stays intact, and I can do it in Ghidra or hand you the byte-level instructions.

Some caveats: I didn't run the program, so this comes from the decompiled and disassembled code and hasn't been reproduced live. Ghidra also reports unsaved changes on `FUN_14002a2c0`, which I didn't cause. I haven't saved anything, so don't save the project unless you want Ghidra's state kept.

Do you want me to go ahead with the patch on a copy of the `.exe`?
