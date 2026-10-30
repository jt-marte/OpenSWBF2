# OpenSWBF2 tests

Headless doctest harness (doctest 1.2.9, vendored). Built with the same VS2022 `v143` toolset as the game (C++17, `/permissive`).

## Build

    MSBuild tests\OpenSWBF2Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32

Output: `bin\Tests\<Config>\OpenSWBF2Tests.exe`. The x64 build (`/p:Platform=x64`, toolset `v120`) writes `bin\Tests\x64\<Config>\` and `obj\Tests\x64\<Config>\`; see "x64" below.

## Toolchain lookup

`tests\vstools.ps1` is dot-sourced by `run.ps1`, `selftest.ps1`, `guard-revert.ps1` and `laa-compare.ps1`. It does one cached `vswhere -latest -version [17.0,18.0)` lookup (requiring the VC x86/x64 tools) and returns MSBuild, dumpbin, editbin and the `HostX86\x86` VC bin directory from that install. If VS2022 with the C++ tools is not found the script prints that and exits 2. Every run starts with a banner: `toolchain:` (the cl.exe version line and MSVC tools version), `ucrtbase:` (the SysWOW64 `ucrtbase.dll` file version) and `windows:` (build and UBR), so a result can be tied to the compiler and CRT that produced it.

## Run

    tests\run.ps1

Builds, then runs every case in its own process. Switches (the only ones):

| Switch | Meaning |
|--------|---------|
| `-Configuration` | `Debug` (default) or `Release` |
| `-NoBuild` | skip the MSBuild step |
| `-Suite` | run one suite (`stress` only runs when named) |
| `-Filter` | wildcard on case name (PowerShell `-like`) |
| `-Fuzz` | run only the `stress` suite, passing seed and count to each case |
| `-Seed` | fuzz seed (default 1) |
| `-Iterations` | fuzz iterations (default 100000) |
| `-Timeout` | per-case seconds (default 10); the runner kills the process after Timeout+10 s |
| `-Platform` | `Win32` (default, behaves exactly as without the switch) or `x64`; see "x64" below |

Results: `artifacts\tests\<timestamp>\results.txt` plus one stdout file per case (`artifacts\` is self-ignored).

## Exit codes and status tokens

Case exe exit codes: 0 pass, 1 fail, 3 harness error (missing input), 77 skip, 124 watchdog timeout, anything else a crash (hex code printed).

Exe prints `[RUN]` at start; the runner prints one of `[PASS] [FAIL] [SKIP] [TIMEOUT] [CRASH] [KNOWN]` per case.
A case that does not report exactly `test cases: 1` with one passed is a FAIL (a zero-match filter cannot go green).
run.ps1 exits 1 on any FAIL/TIMEOUT/CRASH outside known_bug, 2 on setup errors or when zero cases run, else 0.

## Suites

`unit`, `characterisation`, `regression`, `known_bug`, `corpus`, `fuzz`, `stress` (stress is excluded from the default run).
`known_bug` cases document bugs that still exist: their failures print as `[KNOWN]` under a `Known bugs` heading and never fail the run.
A known_bug case that passes prints `WARNING: known_bug case passed, bug may be fixed`.

Test names must be unique and must not contain `,` `*` or `?` (doctest filter syntax).

## GameData tests

Tests that need retail GameData skip cleanly (`[SKIP]`, exit 77) when it is missing. Force the skip path with:

    $env:SWBF2_GAMEDATA='C:\nonexistent'

## Fuzz repro

Failing fuzz cases print their seed and iteration. Replay with
`--fuzz-seed=<n> --fuzz-iter=<i> --fuzz-dump --fuzz-out=<path>` on the test exe.

## Requirements

The preflight is split. T1 needs `d3dx9_43.dll` (DirectX June 2010 redist), `vcruntime140.dll` and `msvcp140.dll` (VC++ 2015-2022 x86 redistributable) in `C:\Windows\SysWOW64`. The game, smoke and guard-revert also need `msvcr120.dll` and `msvcp120.dll` (VS2013 x86 runtime, used by the Galaxy DLLs). Each missing DLL is named and the run exits 2.

## CI runner (manual, one-off)

`.github/workflows/tests.yml` runs on push and same-repo PRs on `[self-hosted, windows, v143]`. Register the runner by hand:

1. Machine with VS2022 (v143 x86 tools, Windows SDK 10.0.26100.0) and DirectX SDK June 2010 (`DXSDK_DIR` set at machine level).
2. Download the Actions runner and configure it: `config.cmd --labels v143` (the `self-hosted` and `windows` labels are automatic; label case may matter, keep them lowercase).
3. Run it as a normal low-privilege user, not NETWORK SERVICE (GameData tests skip if the data is unreadable).
4. Keep no secrets on the box.
5. In the repo settings (public repo) set "Require approval for all external contributors" for fork workflows. The workflow also refuses fork PRs by `if:` guard.
tests/selftest.ps1 runs negative checks of run.ps1 against stub executables; it needs no built test exe, only the same preflight DLLs as run.ps1. It also runs `guard-revert.ps1` with a rows file that throws and with one that selects a row and then makes `Find-GameDataRoot` throw: both must exit 1 with an `ERROR:` line and leave `git worktree list` unchanged. A last scenario checks that `Get-ReparsePoints` reports a nested `mklink /J` junction but not the files in its target, reports nothing for a plain directory, and that `cmd /c rmdir` removes the junction while the target's sentinel file survives.

## Smoke runs

`run.ps1` also launches the real `BattlefrontII.exe` for smoke runs. A plain `tests\run.ps1` runs the unit stage and then the Standard smoke case (geo1c_con). `-Suite`, `-Filter` and `-Fuzz` skip smoke. Naming `-Smoke` runs only smoke.

The Standard smoke also runs `display-switches` (all eight display switches: `-borderless -adapter 0 -refresh 60 -vsync half -fps 120 -nowaitquery -nodpi`) and `display-badini` (`-adapter abc -refresh 99999 -displayini tests\smoke\display-bad.ini`, a checked-in fixture of 5 KB of mixed bytes plus bad `[Display]` keys). Both must reach RUN COMPLETE; under `-norender` the display settings are parse-only.

| Switch | Meaning |
|--------|---------|
| `-Smoke` | `None`, `Standard` (default), `Dedicated`, `Crash`, `All` (every mission in `tests\smoke\missions.txt`, Dedicated and Crash) or `Twice` (Standard twice with `-seed 1`, the two runs must produce the same `finalscore.log` line apart from the timestamp, reported as `[PASS] smoke:run-twice match` or `[FAIL] smoke:run-twice mismatch`; then `nab2g_c` twice, a mission that never ends, until the `OPENSWBF2 SIM DIGEST` run-log line, which carries the mission time and both random seeds after 1500 turns, matches: `smoke:run-twice-digest`; then the digest and trace cases described under "Sim digest") |
| `-SmokeTimeout` | per-case seconds (default 120); on expiry the process tree is killed with `taskkill /T /F` |
| `-SmokeConfiguration` | `Release` (default) or `Debug`; builds `BattlefrontII.vcxproj` unless `-NoBuild` |
| `-Movies` | drop `-norender` so movies and the renderer run |
| `-HogAddrSpace` | append `-hogaddrspace` to every Standard, Dedicated, All-mission and Twice case so the arena lands above 2 GB, and require the `OPENSWBF2 ADDRSPACE` line with `arena=0x8...` or higher, `laa=1`, `steam=1` and non-zero `steam_api`, `Galaxy` and `binkw32` bases (`GalaxyPeer` is only mapped after an online login, so any value passes). Crash cases are unchanged. Off by default; `-Smoke All -HogAddrSpace` takes about 2 hours |
| `-Jobs N` | number of concurrent smoke workers. The default (0) picks half the logical processors, capped at 8 and at least 1; `-Jobs 1` runs serially. Each worker gets its own run folder under the run's artifacts directory (`smoke-workers\wN`) with a copied exe and DLLs, a copy of the `dxvk` folder (so `-Movies` runs render on Vulkan; headless runs log `OPENSWBF2 RENDERER none (headless)` and never load it) and a `data` junction to the staged `bin\Win32\<Config>\data`. Output and results stay in case order. Twice cases and the aslr-base probe stay serial, and a hung case is killed by process tree for that case only. Headless (`-norender`) runs use a message-only window and take no single-instance mutex, which is what lets workers coexist |

Cases run one at a time with the exe directory as the working directory. Without retail GameData, or without the exe and the Bink, Galaxy and Steam DLLs beside it, every selected case prints `[SKIP] ... class=Skipped` and the game is not built. A skipped run exits 0.

Artifacts go to `artifacts\tests\<timestamp>\smoke\<case>\`: `run.log`, `finalscore.log`, any `.dmp` and `result.txt`.

| Class | Meaning | Token |
|-------|---------|-------|
| Skipped | precheck failed | SKIP |
| TimeOut | killed at the timeout | TIMEOUT |
| GsFailure | exit `0xC0000409` | CRASH |
| Fatal | exit 1, 3, 4 or 5, a `FATAL:` log line, or a dump | CRASH |
| Pass | exit 0 and `OPENSWBF2 RUN COMPLETE` in the log, no dump | PASS |
| InitFailure | exit 0 without the marker | FAIL |
| Unknown | anything else | FAIL |

Classes are decided in that order. Crash cases pass when the expected class, exit code and dump count are observed.

Exit code 4 is a Lua panic (an uncaught Lua error). Exit code 5 is a hang: the watchdog saw no frame update for `-hangtimeout` seconds, wrote a dump and a `FATAL: hang` line. The `-crashtest` kinds are `nullderef`, `divzero`, `stackoverflow`, `debugbreak`, `abort`, `invalidparam`, `fastfail`, `arenaoom`, `debugheapoom`, `heapfail`, `luaoom`, `luapanic`, `hang` (spins in the mission loop; the smoke case passes `-hangtimeout 5`) and `check` (calls `ensure(false)` then `check(false)`: exit 3, one dump, `FATAL: check failed`, `ensure summary: 1 failures at 1 sites`).

The reset kinds script the headless fake device (`-norender` only; a real device is never touched). `resetinitlost`: the first `Init` gets `DEVICELOST` from `Reset` three times, then recovers. `resetrecover`, `resetfatal` and `resetflap` arm at frame 30 and drop window focus first, so recovery must not depend on focus. `resetrecover` fails `Reset` three times and recovers. `resetfatal` changes the present params, fails every `Reset` with `INVALIDCALL`, reverts once and exits Fatal. `resetflap` fails 12 times, reports `DEVICELOST` once, fails 12 more times and recovers (`recovered after 25 attempts`, one `RESET failed` line); the 24 failures exceed the 20-failure bound unless a lost period starts a new count. `resetlaterinit` restarts the mission, then in the second `Init` reports `DEVICELOST` for 60 polls while advancing the reset clock by 1 s per poll (60 s, past the 30 s startup bound) and posts `WM_CLOSE` during the wait. The case expects the close to be deferred and run after `Init` recovers, so its expected class is `InitFailure` (exit 0, no `RUN COMPLETE`). The apply `-crashtest` kinds `applymode`, `applymodesmall`, `applyfail`, `applypresentfail`, `applytargetfail` and `applyadapter` also run at frame 30 under the fake device and never arm the reset script. `applymode` asks for 1280x720 (1024x768 if that is the base) in the other window mode with vsync on; it passes when the log has `OPENSWBF2 DISPLAY applied`, `OPENSWBF2 APPLY TARGETS` with at least two render targets at a new buffer size, all three `m_buffers` textures created at their new `BufferSizes` size (`buffers=3/3`), no target left at a size that only the old buffer set used beyond the fixed-size targets counted when the apply was requested (`stale=0`), and a `relayout` line whose screen, view and HUD sizes match the applied size and whose camera aspect matches the screen aspect. The line also lists every tracked size, and a full tracking table logs an Error. `applymodesmall` asks for 320x240, so all three buffers move (`moved=3`), including the 512 and 256 capped ones, and it needs the same target and relayout size checks. `applyfail` fails every `Reset` whose size or window mode differs from the base, so the revert `Reset` succeeds; it passes with a `DISPLAY reverted` line whose used size differs from the asked size, a later `DISPLAY profile restored` line (the snapshot taken when the mission's `Init` succeeded), no `DISPLAY applied`, no `relayout` and no `FATAL:`. `applypresentfail` lets the apply `Reset` succeed and fails the next `Present` once; it passes with the same revert checks and a last `relayout` line back at the used size. `applytargetfail` lets the apply `Reset` succeed and fails every render-target `CreateTexture` larger than the base size, so the recreated targets are missing; it passes with a `DISPLAY reverted ... hr=0x8876017C` line, no `DISPLAY applied`, no `FATAL:` and a last `relayout` line back at the used size. `applyadapter` calls the adapter request with index 1 (invalid, the fake has one adapter) and then 0, with `-displayini` pointed at the case directory; the request only records the choice in memory, so it passes with the invalid Warning, the `pending, running adapter unchanged` line, no `saved` adapter line and no `display.ini` in the case directory (checked with `NoIni`). `adaptercancel` marks the display, requests adapter 0, cancels the way the menu's Cancel does and then saves; it passes when the log shows `pending`, `menu cancel restored` and `adaptertest save=0` in that order, no `saved` adapter line and no `display.ini`. `adapteraccept` arms the same mode change as `menukeep`, requests adapter 0 and saves while the confirm is pending; the menu keys are deferred (`menutest save=1`) but the adapter is written anyway (`adapter=0 saved, running adapter unchanged`), then the confirm times out and reverts; it passes when `display.ini` holds `Adapter=0` and no `Vsync=`. `menuconfirm` arms a mode change at frame 30 that also changes the frame cap and flips wait-query, applies a vsync-only change during the countdown, lets the 15 s confirm time out, and passes when the revert line and the done line show the base fps and wait-query. `menukeep` saves right after arming (the save is deferred, `menutest save=1`), marks again the way the menu's `Enter` does, then keeps the change; it passes when the Keep-time save writes `Vsync=2` to `display.ini` with no later save (`keepsave=0`). `menusavefail` does the same but makes `display.ini` read-only before Keep; it passes when the Keep-time save logs `menu save failed`, the done line has `keepsave=2`, a later save also fails (`menutest save=2`) and `display.ini` has no `Vsync=`. `menuorder` and `menuorderrev` arm the `menuconfirm` mode change without changing the frame cap or wait-query and, in one frame, run the legacy setter's live apply and the menu apply (legacy then menu, and menu then legacy); each passes when exactly one `DISPLAY applied` line sits between its `order=` line and `confirm pending 15s`, followed by a timeout revert and `done result=2`. `menurevertsave` first changes only the frame cap, then arms the `menukeep` change, saves (deferred) and marks again the way the menu's `Enter` used to, lets the confirm time out, then cancels the way the menu's Cancel does; it passes when the revert finishes the deferred save (`menu saved keys=0x10`, `display.ini` holds the new `Fps=` and no `Vsync=`) and the cancel leaves the pre-apply vsync with the accepted frame cap. `menuapplyfail` makes the fake device fail every `Reset` away from the mode that was running when the confirm-armed stage was made; it changes only the frame cap, then arms a mode change with a second frame cap and a flipped wait-query, and saves (deferred). It passes when the failed apply restores the accepted frame cap and the base wait-query (`profile restored ... fps=<accept>`), finishes the deferred save (`menu saved keys=0x10`, `display.ini` holds only the accepted `Fps=`), and the confirm never starts.

`-logcmds` takes one quoted value, for example `-logcmds "all Warning, LogNet Verbose"`. The categories are `LogNet`, `LogLua`, `LogLoad`, `LogMemory`, `LogSim`, `LogRender`, `LogAudio`, `LogCore` and `LogTemp`. Every log line starts `[t=<seconds>] [f=<frame>] <Category> <Severity> <file>:<line>: `. When a debugger is attached (Rider or Visual Studio F5) every line that reaches `run.log`, including Lua `print`, is also sent to `OutputDebugString` and shows in the IDE debug output window; the T1 case `debugger output mirrors the log` proves it by running the test exe as its own debuggee.

### Sim digest

At turn 1500 the game logs `OPENSWBF2 SIM DIGEST turns=<n> time=<mission seconds> rand=<seed a>/<seed b> snd=<sound seconds> state=0x<hash>`. `state=` is an FNV-1a 32 hash over the raw bits of the mission time, characters, teams, command posts, AI goals and placed objects (`src\Battlefront2\Source\SimDigest.cpp`), so it changes when any of that drifts, even where `rand=` does not.

`tests\smoke\digest-expected.txt` holds one `case<TAB>full digest line` row per digest case; blank lines and lines starting with `#` are ignored. `Test-SmokeExpect` compares the case's digest line with its row (exact, case-sensitive) and fails with `digest expected:` and `digest actual:` lines on any difference. A case without a row is not checked. The rows were written once from the VS2013 Release build, are unchanged on v143 (see `tests\baseline\v143\CAUSES.md`) and are enforced only when `-SmokeConfiguration` is `Release`. Change a row only with a written per-mission cause. `OPENSWBF2_DIGEST_EXPECTED` points the compare at another file, which the selftest uses to prove a one-bit `state=` flip fails, and which an empty file turns into a record-only run. Rows exist for the Standard `geo1c_con` case, the Twice cases `twice-digest-1/2` (nab2g_c), `digest-hot1g_con`, `digest-spa3c_ass` and `trace-seed-1`, and the `-Smoke All` cases `nab2g_c`, `hot1g_con` and `spa3c_ass`, whose arguments the selftest proves equal to the Twice cases. The Standard group also runs `digest-dag1c_ctf` (a mission that never ends, stopped after the digest line) and `digest-kas2c_uber`, whose rows guard causes C1 and C2 (see `tests\baseline\v143\CAUSES.md`): their turn-1500 digests change when either fix is reverted, which the guard-revert rows `SIMFIX-C1S` and `SIMFIX-C2S` prove. The same rows check the `-Smoke All` cases `dag1c_ctf` and `kas2c_uber`, whose arguments the selftest proves equal to the Standard cases. The digest is the same with `-HogAddrSpace`.

`-digesttrace N` logs `OPENSWBF2 DIGEST TRACE turn=<t> state=0x<hash> rand=<a>/<b>` for turns 1 to N. `-factorylog` logs the Factory creation order (`OPENSWBF2 FACTORY <entity|ordnance|weapon|explosion> idx=... id=... parent=...`) on the first mission frame and an `OPENSWBF2 FACTORY COUNTS` line then and at the digest. Neither switch changes the digest.

The Twice group runs `trace-seed-1` and `trace-seed-2` (hot1g_con with `-seed 1` and `-seed 2`, `-digesttrace 1600`). `Compare-DigestTrace` in `smoke.ps1` takes two `run.log` files and returns the first turn whose `state=` differs (0 when every turn matches, -1 when either log lacks a trace line for a turn before the split); `rand=` is not compared. `run.ps1` reports it as `[PASS] smoke:digest-trace split at turn <t>`, `[FAIL] smoke:digest-trace no split` or `[FAIL] smoke:digest-trace incomplete trace`, and also checks that the trace line at the digest turn of `trace-seed-1` carries the digest's `state=` (`smoke:digest-trace matches digest at turn <t>`). `tests\guard-revert.ps1 -Row DIGEST-01` changes one bit of the FNV offset basis and must turn `geo1c_con` RED.

### Log strictness

A smoke case also fails on any Error, Bug or Fatal `[t=` line in its `run.log` (only that file, matched case-sensitively), any `ensure failed: ` line, or a legacy `Message Severity: 3`, `4` or `5` line, unless a regex in `tests\smoke\expected-errors.txt` covers it. The file has `[global]` and `[case-name]` section headers with one regex per line; blank lines and lines starting with `#` are ignored. `[global]` is empty. Unprefixed Lua `print` output never counts. The T1 test exe is strict in the same way: `ExpectLog("text")` and `ExpectEnsure("text")` (named RAII variables) declare an expected Error-or-above line, an undeclared one or an unmatched declaration makes the exe exit 1 at the end of `main`. The hardening step also fails if any file under `src\LuaFL` or `src\ZlibFL` mentions `RedLog.h` (`hardening:redlog-vendor`).

The runner prints `WARNING:` lines when the crash dump setup is missing. Import `tools\wer-localdumps.reg` once as administrator (HKLM only) so a `/GS` fast-fail leaves a dump and no Windows error dialog. The runner never writes the registry.

With `-Movies` the retail `vidmode.ini` in `data\_lvl_pc` decides the video mode, so a stale or fullscreen entry can change what the run shows.

## x64

`-Platform x64` is accepted by `run.ps1`, `netpair.ps1`, `guard-revert.ps1`, `laa-compare.ps1` and `selftest.ps1`. Leaving it out is Win32, with the same paths, output and test count as before.

- `run.ps1 -Platform x64` builds `OpenSWBF2Tests.vcxproj` for x64 (the same as `MSBuild tests\OpenSWBF2Tests.vcxproj /p:Configuration=Debug /p:Platform=x64`; toolset `v120`, `OPENSWBF2_NO_GALAXY` and `OPENSWBF2_FFMPEG` defined, the Windows 8.1 SDK beside the DirectX SDK `Lib\x64`) and runs `bin\Tests\x64\<Config>\OpenSWBF2Tests.exe`. The preflight looks for `d3dx9_43.dll` and `msvcr120.dll` in `System32` (the x64 DirectX June 2010 redist and vcredist 2013 x64). A failed build prints `Build failed (OpenSWBF2Tests, Platform=x64).` and exits 2. Skipped cases print their reason under the `[SKIP]` line.
- The smoke and hardening steps use `bin\x64\<SmokeConfiguration>` and `obj\x64\<SmokeConfiguration>`. While `BattlefrontII.vcxproj` has no `<Config>|x64` configuration, or the exe is not built and `-NoBuild` is given, they print `[SKIP]` lines saying so and the run still exits 0. The smoke precheck asks the exe (`dumpbin /dependents`) which DLLs it needs instead of demanding the Bink, Galaxy and 32-bit Steam DLLs, and parallel workers copy every DLL beside the exe. Expected `FATAL:` crash signatures in the `-Smoke All` list do not pin the `d3dx9_43.dll` offset on x64 (the x64 DLL has a different one).
- Large-address-aware checks have no meaning on a 64-bit image and print `[SKIP]`: `hardening:laa` and `hardening:safeseh` (SafeSEH is x86-only), `-HogAddrSpace` (ignored with a `[SKIP] smoke:hogaddrspace` line), `laa-compare.ps1` (exits 0 with `[SKIP] laa-compare`) and the `LAA-*` rows of `guard-revert.ps1`. The ASLR probe compares the image base with the x64 default `0x140000000`.
- VS2013 reports pointer truncation (C4311, C4312, C4302) on x64 only at `/Wall`, so a plain `/W3` build hides it. The x64 test project passes `/w34311 /w34312 /w34302` to bring those three to level 3; the warnings are not errors.

### x64 baselines

x64 float codegen and libm differ from x86, so the expected values are kept per platform. The Win32 files stay where they are and are never read for x64.

| What | Win32 | x64 |
|------|-------|-----|
| Sim digests | `tests\smoke\digest-expected.txt` | `tests\smoke\x64\digest-expected.txt` |
| Math corpus | `tests\corpus\mathfn\math.txt` | `tests\corpus\mathfn\x64\math.txt` |
| Float parse corpora | `tests\corpus\floatparse\*.txt` | same files (x64 parses identically; the cases check it) |
| VS2013 reference capture | `tests\baseline\vs2013` | none (a one-off Win32 record, no run reads it) |

When an x64 baseline is missing the runner does not fail. A missing digest file or row prints `[SKIP] smoke:digest - no x64 baseline yet in <file> for: <cases>. Record one with: ...` and the digest is left unchecked; a missing math corpus makes `math corpus` print `[SKIP]` with the same advice. `math corpus x87 pc24` is skipped on x64 (no x87 precision control).

The x64 math corpus is checked in. The x64 digest file is not: it needs a working x64 game build, so until someone records it the digest checks print the `[SKIP]` above. Record them from a good Release x64 build:

    tests\run.ps1 -Platform x64 -Smoke Standard -RecordDigests
    tests\run.ps1 -Platform x64 -Smoke Twice -RecordDigests
    tests\run.ps1 -Platform x64 -Smoke All -RecordDigests
    tests\run.ps1 -Platform x64 -RecordCorpus

`-RecordDigests` runs the selected smoke cases without comparing, and for every passing case that has a row in the Win32 file it merges the case's `OPENSWBF2 SIM DIGEST` line into `tests\smoke\x64\digest-expected.txt` (the file is created with a header, written CRLF, and rows already there are replaced). It needs `-Platform x64`, a smoke run and `-SmokeConfiguration Release`. `-RecordCorpus` builds the x64 test exe and runs `OpenSWBF2Tests.exe --gen-corpus=corpus`, which on x64 writes only `corpus\mathfn\x64\math.txt` (no GameData needed). Both refuse to run for Win32, whose baselines are pinned. Review and commit the changed files like any other baseline change.

The x64 test exe calls `_set_FMA3_enable(0)` first: msvcr120's x64 libm takes FMA3 code paths on CPUs that have them, which changes some results in the last bit (17 of the 10636 math corpus lines on the AVX2 machine the baseline was recorded on; 225 lines of the recorded x64 file differ from the x86 file). Pinning the generic SSE2 path keeps the recorded corpus the same on every machine. A game binary that does not do the same gets machine-dependent `sin`, `cos`, `pow` and friends, and with them machine-dependent sim digests.

## Network pair runs

`tests\netpair.ps1 [-Profile clean|lag100|lag200|jitter|all] [-Clients N] [-Seconds 45] [-Direct] [-ServerArgs "/tps 60"] [-Map geo1c_con] [-Configuration Release] [-Platform Win32|x64] [-ClientPlatform Win32|x64]` is opt-in and not part of `run.ps1`. It starts a headless dedicated LAN server (`/autonet dedicated -lan`) in its own worker folder, waits for its `OPENSWBF2 NET mission start` line, then starts N headless autonet clients that find it by LAN broadcast and join (with `-Direct` they skip discovery and join `127.0.0.1` by address through `/connect`, the Join IP path). After every client logs `mission start` it lets them play for `-Seconds`, kills the process trees and reads the logs. `-ServerArgs` adds switches to the server's command line (for example `/tps 60`). `lag100` and `lag200` pass `-latency`/`-loss` to the server, which pushes them to its clients; `jitter` passes `-latency 60 -jitter 50 -loss 1` (each packet 60 ms plus or minus 30 ms each way: `-jitter` is a percentage of `-latency`). Clients started with `-ClientArgs -nettestplayer` spawn by themselves and play a fixed pattern of moves (walk, strafe, steady aim changes, fire), so the host gets real inputs to guess, replay and lose. A run fails on any `align mismatch`, `bad soldier state`, `bad walker state`, `drop`, `host lost`, `join lost`, `join refused`, `join rejected` or `FATAL:` line, an early exit, server and client `characters` lines that differ, or an average below the profile's update floor (clean 8/s, lag100 3/s, lag200 2/s, jitter 3/s). It prints `[PASS]`/`[FAIL] netpair:<profile> join=lan|direct clients=N updates=<avg/s> ping=<ms> loss=<%> fills/s=<turns the host guessed> replayed/s=<late moves replayed> lost/s=<guessed turns never repaired> corr=<average>/<largest correction, m>` and writes logs to `artifacts\netpair\<stamp>\<profile>\`. It skips (exit 0) without GameData or without a private LAN address. With `-Direct` every process also gets `-netloopback`, so its sockets listen on 127.0.0.1 only and Windows Firewall never prompts for the worker copies of the exe. `-ClientPlatform` runs the clients on the other build, for mixed Win32 and x64 games (the server stays on `-Platform`).

Network log lines start `OPENSWBF2 NET` (category `LogNet`, Inform unless noted): `socket open host=<0|1> port=` (`loopback=1` with `-netloopback`), `broadcast open port=3656` (Warning on a failed bind), `host registered lan addr=`, `join sent to=`, `join accepted from= player= guests=` (`late=1` for a join into a running mission), `join rejected from= reason=full|password|version`, `join refused reason=` (client side), `joined player=`, `mission start host= dedicated= tps= game=`, `characters total= players= bots=` (the character table size, which host and clients must agree on), `direct find <ip:port>` and `join ip: cannot use address` (Warning), `drop player= reason=quit|timeout|load|boot silent=`, `host lost silent=`, `join lost silent= player=`. Desync detectors log a Warning: `align mismatch` (the per-object alignment marker read back wrong, which retail release builds never checked; at most 8 per process), `bad soldier state` and `bad walker state` (an index the local object cannot use; the rest of that update is dropped instead of crashing), and `powerup station state host= local=`. LAN discovery logs `lan find` and `lan found` at Verbose. `-netstats` adds one line a second in a mission: clients log `NET STATS client player= ping= updates= in= out= loss= lag= ts= corr=` (kbit/s in and out, lag in turns, timescale, and the average/largest jump in metres when an update corrected a remote soldier's predicted position, respawns excluded), hosts log `NET STATS player= ping= updates= fills= loss= fills_s= same_s= fixed_s= lost_s= fire_s=` per remote player (per second: turns filled with a guess because the move was missing, late moves that matched the guess, late moves replayed, guessed turns never repaired, and replayed moves whose fire buttons differed from the guess) and `NET STATS host players= in= out= replays_s= shots_s= replay_shots_s= events_s= stale_s= split_s=` (turns replayed, shots created and those on replayed turns, shot events sent to clients and dropped as stale, and updates that needed a second packet).

## AI runs

`-aistats` logs `OPENSWBF2 AI STATS t= level= target_human=<%> (<s> of <s>) contested_human=<%> (<s>) veh_contested_human=<%> (<s>) kills bot>bot= bot>human= human>bot= ai_ms= max= posts t1=<posts>/<share>% t2=...` (category `LogSim`) every 30 s of mission time and once before `RUN COMPLETE`, on the host only. The seconds are bot think time summed over all bots: `target_human` is the share spent aimed at a human, `contested_human` the share aimed at a human while the bot could also pick a bot enemy (`veh_` counts only bots in vehicles and turrets), `ai_ms` the average and largest time per frame in `ControllerManager::Update`, and `posts` per team the number of command posts its living bots are assigned to and the largest share of them on one post. `-aitesthuman` makes the single-player human respawn by itself as a random class (when the team has one) and stand still, a stand-in human for headless runs. The Standard smoke case `ai-overhaul` runs `geo1c_con` with `/skill 4 -aistats -aitesthuman` and needs a `level=4` stats line with at least one bot-on-bot kill.

## LAA comparison

`tests\laa-compare.ps1 [-Runs N] [-Configuration Release] [-NoBuild] [-Jobs 8] [-OutDir path]` is opt-in and not part of `run.ps1`. It copies the built exe, patches only the copy with `editbin /LARGEADDRESSAWARE:NO`, then runs the Standard mission `-Runs` times (minimum 5) with the patched copy and `-Runs` times with the real exe plus `-hogaddrspace`, in parallel worker folders. It checks both headers with `dumpbin`, checks the arena sits below 2 GB for the patched copy and above it for the real exe, and compares the `finalscore.log` key and the `[f=N]` frame count of every run. The key and frame count must be identical across all runs on both sides. Nothing is masked beyond the date and time prefix. It prints `[PASS] laa-compare` or `[FAIL] laa-compare` (exit 1) and skips with exit 0 when GameData or DLLs are missing. 5 runs per side take about 20 seconds with 8 jobs.

## Hardening

`tests\run.ps1 -Hardening` builds the game (no GameData needed) and runs only the hardening step: `dumpbin /headers` and `/loadconfig` on `bin\Win32\<SmokeConfiguration>\BattlefrontII.exe` plus a count of `/GS-` in the cl tlog, which is also checked for `/sdl` on every entry. It prints one `[PASS]`/`[FAIL] hardening:<flag>` line each for dynamicbase, nxcompat, laa, reloc, safeseh, gs and sdl, and any missing flag fails the run. The same tlogs feed four flag checks: `flag-alignednew` and `flag-inline` require `/Zc:alignedNew-` and `/Zc:inline-` on every game cl entry (VS2013 behaviour: no aligned `operator new` overloads, unreferenced inline COMDATs kept), `flag-alignednew-tests` requires `/Zc:alignedNew-` on every T1 cl entry (`[SKIP]` when the tests are not built), and `flag-banned` fails on any `/Zc:throwingNew`, `/GL`, `/fp:fast`, `/fp:contract` or `/GS-` in either project. dumpbin comes from `tests\vstools.ps1`; if VS2022 is not found the run exits 2. The default run also performs this step.

With GameData, `hardening:aslr-base` launches the game once and requires `MainModule.BaseAddress` to differ from 0x400000. Windows picks the base once per boot, so two launches are not compared. Without GameData the line is `[SKIP]`.

`-Smoke All` reads `tests\smoke\missions-all.txt` (name and expected class: Pass, TimeOut or Fatal). TimeOut cases run the full `-SmokeTimeout` and pass only if the log shows `OPENSWBF2 MISSION RUNNING`, which the game writes on the first mission frame with `-exitongameover`. A `GsFailure` always fails. Every smoke case also passes `-nofocuspause`, so the game keeps running when its window loses focus (no pause, minimise or inactive sleep), which lets a `-Movies` run sit in the background. Headless (`-norender`) runs create a hidden window and the smoke script runs the game at BelowNormal priority, so a run does not take focus or slow the desktop.

`tests\guard-revert.ps1` proves the T1 regression tests fail without their fix. It is a developer tool and is not run by CI. It adds a detached git worktree under `artifacts\guard-revert\wt` (never under TEMP, where MSBuild misses header edits), overlays the current working tree on it, builds T1 once as the control, then for each row in `tests\guardrevert\rows.ps1` reverts one guard (a literal anchor that must occur exactly once), rebuilds and runs the named cases. Run it with `powershell -File tests\guard-revert.ps1`, or `-Row LOCAL-01,NETF-03` for a subset; `-Keep` leaves the worktree. In the table, `Control` must be GREEN and `Reverted` must be RED (the case failed, crashed or timed out). STILL-GREEN means the test cannot detect the revert, WRONG-RED means a smoke row with a `RedRx` pattern failed without that pattern in its log (it failed for some other reason), NO-MATCH means the case name is wrong, BUILD-FAIL or ERROR means the row itself is broken. `Kind = 'Tlog'` rows (`FLAG-ALIGNNEW`, `FLAG-INLINE`, `FLAG-ALIGNNEW-T`, `FLAG-THROWNEW`, `FLAG-GL`, `FLAG-FPFAST`, `FLAG-FPCONTRACT`, `FLAG-GSMINUS`) edit the project's `AdditionalOptions`, rebuild that project and must turn the named `flag-*` check RED. `NANGUARD-01` and `NANGUARD-02` revert the ODF value guard itself (non-finite rejection and dangling-exponent repair); `NANGUARD-03..05` remove its call sites in `Entity.cpp`, `LoadUtil.cpp` and `LuaCallbacks_Mission.cpp`, which the T1 case `odf value guard covers every property call site` detects by reading the loader and Lua sources. The exit code is 0 only if every row is RED, and the table is saved to `artifacts\guard-revert\<stamp>\summary.txt`. Any terminating error (a throwing rows file, a failed build or copy) prints `ERROR:` with the script location and exits 1, and so does a table with fewer rows than were selected, which names the missing row Ids. The worktree is removed only after `Get-ReparsePoints` (in `smoke.ps1`) finds no junction or symlink in it: then `git reset --hard`, `git clean -fd` and `git worktree remove` without `--force`. If any reparse point is found, the worktree is left in place with a `REFUSING to remove` line listing them; if `git worktree remove` fails it is left in place with a `Cleanup failed` line. Both exit 1. The main tree is never modified; the script reports any change in `git status`.

The fresh-install default resolution follows the chosen adapter's desktop size (D-04); saved profiles and -norender/smoke runs are unchanged.

## Baseline capture

`tests\baseline\capture-vs2013.ps1` records the VS2013 reference for the toolchain move. It builds Release unless `-NoBuild` is given, then runs the selected steps: `-Map` (initializer order from the `.map` `.CRT$XC*` range, demangled), `-Layouts` (`/Zs /d1reportSingleClassLayout` dumps with the build's own cl flags), `-Collect` (factory logs, 1500-turn digest traces and digests for geo1c_con, nab2g_c, hot1g_con and spa3c_ass, plus their `-hogaddrspace` digests), `-Tables` (copies the smoke, hardening and guard-revert tables from `-SmokeAllDir`, `-HardeningDir` and `-GuardRevertDir`), `-DebugDigests` (Debug digests from `-DebugSmokeDir`), `-Archive` (a SHA-256 listed zip plus `MANIFEST.md`) and `-Verify` (rechecks every hash and fails on unlisted files). `-All` runs map, layouts, collect, archive and verify. Text output goes to `-OutDir` (default `tests\baseline\vs2013`) with the repo root and `%USERPROFILE%` scrubbed. The zip goes to `-ArchiveDir`, else `OPENSWBF2_BASELINE_DIR`, else `%USERPROFILE%\OpenSWBF2-baseline\vs2013`; a path inside the repo is refused after junctions, `subst` drives and 8.3 names are resolved, and binaries are never committed. `MANIFEST.md` records the scrubbed archive path, and `-Verify` looks there unless `-ArchiveDir` is given. The `-Layouts` tree-unchanged check ignores changes under `-OutDir`.

`capture-vs2013.ps1` captured the VS2013 reference before the toolchain move; that part is history. Its `-Collect`, `-Map`, `-Tables` and `-DebugDigests` steps also produced the v143 capture in `tests\baseline\v143` (`-OutDir tests\baseline\v143`). `-Archive` and `-Verify` stay VS2013-only. Every v143 difference from VS2013 has a written cause in [tests/baseline/v143/CAUSES.md](baseline/v143/CAUSES.md).

The `corpus` suite also checks the float parse and math corpora under `tests\corpus\floatparse` and `tests\corpus\mathfn`; see `tests\corpus\README.md` for their format. The corpora are v143-generated; the VS2013 originals are under `tests\baseline\vs2013\corpus`.

## ODF value guard

UCRT `sscanf`/`atof` parse `nan` and `inf` tokens that the VS2013 CRT rejected (`sscanf` returned 0 and left the target untouched), and read dangling exponents differently. `OdfValue::Sanitize` (`src\Battlefront2\Source\OdfValue.cpp`) rewrites a property value only when it is a whole token that starts with a letter (after an optional sign), `strtod` gives a non-finite result and `strtod` consumes the whole token. So `nan`, `inf`, `infinity`, `nan(...)` and their signed or case forms give the VS2013 result, and dangling exponents (`1e`, `1e+`) read as 1.0. String values such as `info`, `infantry_x.msh` and `inf_trooper` are never touched. Accepted residual: a float field holding junk that only starts with `inf` or `nan` (for example `infx`, `nan(`) still loads as non-finite under UCRT. Overflow like `1e39` keeps the VS2013 result (inf).
