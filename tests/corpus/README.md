# Corpus

One input per file, raw bytes, under `tests\corpus\<reader>\<name>.bin`. Files are named after
what they trigger. The files are the only binary content in `tests\`; `.gitattributes` marks
`*.bin` as binary. Inputs are replayed on every run by the `corpus` suite and are also the base
material for the fuzz mutator.

## recvgroup

A sequence of records until end of file. All integers are little endian.

    u16 group, u8 type, u8 src, u8 idx, u8 count, u8 compressed, u16 payloadLen, payload[payloadLen]

The record header is 9 bytes. `payloadLen` must be <= 998; a record that claims more, or one
truncated by end of file, ends the parse. At most 128 records are used and the file is capped at
64 KB. Each record becomes one NetPacket stamped with the header and queued on `inQueue`, then
`NetComm::Recv` is drained for every packet type.

## recvpacket

One raw LAN datagram as it arrives from the socket, the 3 byte LAN prefix implied:

    [type][group lo][group hi][idx][count | 0x80 if compressed] payload...

It is fed through `NetCommP::MakeReceivedPacket` and `NetCommP::RecvPacket`, then drained with `NetComm::Recv`.
Inputs of any length are replayed, including 0 bytes. `empty-datagram`, `short-datagram-3` and `pool-flood` cover the ingress length checks and the receive pool reserve.

## known_bug

Inputs in a `known_bug\` subdirectory trigger a bug that exists today. There are none at the moment; the RecvGroup
infinite-loop inputs (`frag-idx-ge-count`, `frag-classic-gap`) moved up to `recvgroup\` when it was fixed. The plain replay case skips them. Each has its own `known_bug` suite case, which
run.ps1 reports as `[KNOWN]` while it fails or times out and warns about once it passes. When the
bug is fixed, move the file up one directory and delete its `known_bug` case in the same change. A
case whose input is missing exits 3 and reports `[CRASH]`, never `[KNOWN]`.

## NetWire readers

`chat-client`, `chat-server`, `upload`, `rcon`, `load` and `turnrate` hold the bytes after the comm header. `ReplayNetWireBytes` writes the file into a `NetPktGroup` with `WriteBytes`, then reads it back with the matching `NetWire` helper into a canary filled buffer (0xAA) and checks the pool is whole (512 packets).

- `chat-*`: u32 header (`0xffff0000 | length`), then the payload, then a sentinel word. The low 16 bits of the header are the claimed size.
- `upload`: replayed twice per file, once as a chunk (10 bit length in the first word, data from the next word) and once as a name (u8 length then characters).
- `rcon`: a 9 byte header with the command at byte 5, a zero at byte 6, the key at byte 7 and the claimed line length at byte 8, then the line. The whole file also goes through the authorization check.
- `load`: 7 bit player count in the first word. `turnrate`: 10 bit duration then 3 bit turns.

`corpus generate netwire` regenerates the files when `OPENSWBF2_GEN_CORPUS=1`. The `fuzz netwire readers` case mutates these files with its own seeds and pinned checksums.


## floatparse

Text data, not `.bin`: one token per line, CRLF, tab between the token and the fields, a space
between fields. Bits are upper-case hex.

    <token>\t<ret> <fbits %08X> <atof %016I64X> <fatof %08X> <strtod %016I64X> <consumed>

`ret` and `fbits` are the return value of `sscanf(token, "%f", &f)` and the float bits, with `f`
filled with `7FA5A5A5` beforehand so a failed conversion shows. `atof` is the double result of
`atof(token)`, `fatof` the bits of `(float)atof(token)`, `strtod` the double result of
`strtod(token, &end)` and `consumed` is `end - token`.

- `odf.txt`: every distinct numeric token in the retail lvl `PROP` chunks, sorted bytewise.
  The game reads ODF values from `PROP` chunks inside the lvl files (`entc`, `ordc`, `wpnc`,
  `expc` class chunks; `EntityClass::Read` -> `SetProperty` -> `sscanf("%f")`), so the tokens
  come from those chunks only. The values are split on whitespace, quotes and commas are
  stripped, and only tokens matching `^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$` are kept. The file
  holds numbers only, no names or strings. 191 lvl files, 125,744 PROPs, 784 tokens.
- `edge.txt`: hand-picked edge cases. The loose `.cfg`/`.ini` numeric paths go through the same
  `sscanf("%f")` and `atof` entry points, and `edge.txt` covers them with the long-mantissa,
  near-halfway, exponent, leading `+`/`.` and trailing-junk cases, plus `nan`, `inf`, `1.#INF`
  and `1.#QNAN` forms recorded as the generating CRT parses them.

## mathfn

`math.txt`, text data, CRLF:

    <form>\t<in1 bits> [<in2 bits or decimal int>]\t<out bits>

Float bits are `%08X`, double bits `%016I64X`. `<fn>.f` is the C++ float overload as game code
writes it (`sin((float)x)`), `<fn>.d` the double call, for `sin cos tan asin acos atan sqrt exp
log log10 floor ceil`, plus `atan2.f`, `atan2.d`, `fmod.f`, `fmod.d`, `pow.ff`, `pow.dd`,
`pow.fi` (float, int) and `pow.di` (double, int). Inputs are a grid over each function's sim
domain plus edge values (signed zeros, `FLT_MIN`, denormals, `FLT_MAX`, multiples of pi, `1e6`,
`1e10`, NaN, infinities; `1e300`, `DBL_MIN` and `DBL_MAX` for the double forms). The checker
reads the input bits from the file at run time, so the compiler cannot fold them.

`math corpus x87 pc24` replays the file under `_PC_24`, the x87 mode Direct3D sets without
`D3DCREATE_FPU_PRESERVE`. Optimized code binds the double calls to `__libm_sse2_*_precise` and
`__CI*`, which ignore x87 precision, so in Release every line matches. Unoptimized code (T1 Debug,
and `NetGame.cpp`/`NetSetup.cpp` in the game) calls the plain msvcr120 entries, and `sqrt` and
`atan` there follow x87 precision: the case allows only `sqrt.d` and `atan.d` lines to differ and
prints how many did (273 in Debug, 0 in Release).

## mathfn on x64

The x64 build checks `mathfn\x64\math.txt` instead of `mathfn\math.txt`: the x64 libm rounds some results differently from x86 (225 of the 10636 lines differ), so each platform keeps its own file and the Win32 one is never read for x64. The format and the input grid are the same. The test exe pins the generic SSE2 path with `_set_FMA3_enable(0)` so the file does not depend on whether the CPU has FMA3. `math corpus x87 pc24` is skipped on x64. If the x64 file is missing `math corpus` prints `[SKIP]` instead of failing.

Record it with `tests\run.ps1 -Platform x64 -RecordCorpus` (or `OpenSWBF2Tests.exe --gen-corpus=corpus` from `tests\`). On x64 that writes only `mathfn\x64\math.txt`, needs no GameData, and gives the same bytes from the Debug and Release builds. `floatparse` is shared by both platforms: the x64 build parses every token identically, and the `float parse corpus` cases check it.

## Provenance of floatparse and mathfn

Generated on v143 (MSVC 14.44.35207, `ucrtbase.dll` 10.0.26100.9711) by running
`..\bin\Tests\Release\OpenSWBF2Tests.exe --gen-corpus=corpus` from `tests\` with retail GameData
present (without it the mode prints `[SKIP]` and exits 77). The VS2013 Update 5 originals (`cl`
18.00.40629, `msvcr120.dll` 12.00.40664.0) are kept unchanged under
`tests\baseline\vs2013\corpus\`. `odf.txt` is identical on both toolchains; the `edge.txt` and
`math.txt` deltas are explained in `tests\baseline\v143\CAUSES.md`. The files are regenerated
only at a toolchain change, with a written cause. The checker cases in the `corpus` suite read
only these files and need no GameData, so they never skip (on x64 `math corpus` skips until
`mathfn\x64\math.txt` exists). `mathfn\x64\math.txt` was first generated on VS2013 Update 5 for
x64 and regenerated the same way on v143 x64 when the x64 build moved to v143.
