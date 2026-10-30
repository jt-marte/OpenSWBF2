# v143 against the VS2013 baseline

Every difference between the v143 Release build and `tests/baseline/vs2013/`, with its cause.

## Toolchain

| Key | VS2013 | v143 |
|-----|--------|------|
| toolset | v120_xp | v143 |
| cl | 18.00.40629 | 19.44.35229 |
| MSVC tools | 12.0 | 14.44.35207 |
| CRT | msvcr120.dll 12.00.40664.0 | ucrtbase.dll 10.0.26100.9711, vcruntime140.dll 14.51.36247.0 |
| Windows SDK | 7.1A | 10.0.26100.0 |
| Windows build | | 26200.9711 |
| DXSDK | June 2010 | June 2010 |

Two builds are named below. The Phase 20 build is the first v143 build (commit 1b1cbca, exe
SHA-256 `403C48C2...86BB14ED`). The fixed build is the main tree after 20-14 (commit a2c980c,
exe SHA-256 `17A3A864...30A71885`), which neutralises C1, C2 and C3 and fixes C-OOB-SIDE (see
`## Causes`). The tables in this directory come from the fixed build.

## Digest missions

`Compare-DigestTrace tests\baseline\vs2013\digest-trace-<m>.txt tests\baseline\v143\digest-trace-<m>.txt`
returns the first split turn (0 = equal). Each v143 trace has 1600 turns. The split turns were
re-measured on the fixed build.

| Mission | Final digest (turn 1500) | First split turn | Cause |
|---------|--------------------------|------------------|-------|
| geo1c_con | same | 0 | none in the window; C2 after turn 1600, see below |
| nab2g_c | same | 0 | none |
| hot1g_con | same | 0 (1397 on the Phase 20 build) | C1, fixed; see below |
| spa3c_ass | same | 0 | none in the window; C2 after turn 1600, see below |

`digests.txt` and `digests-hog.txt` are byte-identical to VS2013, and every `state=` matches
`tests/smoke/digest-expected.txt`, where no row has changed. No reference digest moved in Phase 20.
In 20-15 the Standard group gained `digest-dag1c_ctf` and `digest-kas2c_uber`, whose rows are the
VS2013 turn-1500 digests of the C1 mission dag1c_ctf (first split 202) and the C2 mission
kas2c_uber (first split 636); guard-revert rows SIMFIX-C1S and SIMFIX-C2S turn them RED when the
C1 or C2 fix is reverted.

hot1g_con: on the Phase 20 build only turns 1397 and 1398 differed, and only in `state=`
(`0x1A9B6C97` vs `0x6D1DD393`, `0x38735EB1` vs `0xAFCEE16D`). `rand=` was identical on every turn,
and turn 1399 onward was identical. Field-level logging built under both toolchains on the same
staged data pins it to character 32 (team 2, world key 554088578): `position.y` was 1 ulp lower on
v143, `BF22BD23` vs `BF22BD24` at turn 1397 and `BF247374` vs `BF247375` at turn 1398 (about
-0.636). Velocity, health, every other character and world object, all command posts and the
mission time were identical. At turn 1398 the velocity is `00000000 BF7A7EA0 80000000`, a
landing/settle frame, and the next turn writes the position back to the same value on both
builds, so the run reconverges at 1399. The cause is C1: the NEUTRAL-C1 build removes both turns
and NEUTRAL-C2 leaves them. On the fixed build `digest-trace-hot1g_con.txt` equals the VS2013
trace on all 1600 turns.

Window: the 4 reference digests match VS2013 through turn 1600 (hot1g_con 1397-1398 excepted on
the Phase 20 build, as above). On the Phase 20 build geo1c_con first split at turn 1629 (first
`rand=` split 2911) and spa3c_ass at turn 2445 (first `rand=` split 35967), both after the
1600-turn trace window. Those later splits are why their final scores differed while their
turn-1500 digests and reference traces matched (see `## Final score vs VS2013`). Both later
splits are C2. On the fixed build both equal VS2013 in every turn to game over, geo1c_con over
all 9014 turns.

The `<cmath>` float overloads are load-bearing. A v143 build without them splits geo1c_con at turn
127 and changes its turn-1500 digest (`state=0xED7EDAC9` instead of `0x16D65714`).

## Causes

Every v143 difference from VS2013 in the sim and in `finalscore.log` has one of these four
causes. C1 and C2 were found with the D-15 bisect (code-bug screen, then `<cmath>`, `/fp`, CRT,
per-TU toolchain) on the first split of dag1c_ctf and geo1c_con. Each mission was then attributed
by experiment, see `## -Smoke All vs VS2013`. C3 was found with the same bisect on the one
geo1c_xl turn that still differed with C1 and C2 neutralised. The evidence (field and stage
traces, neutral builds, probes) is kept under `artifacts\gap20`, which is not tracked.

### C1

- Function: `RedTerrain::GetInterpolatedHeight` in `src/RedEngineFL/Application/RedTerrain.cpp`
  (TU `RedTerrain.cpp`), `// FUNCTION: SWBF2 0x006c1720`, line 241 on the Phase 20 build: the last
  branch, `fH = (GetHeight(iX + 1, iZ + 1) - fH0) * fFracZ`.
- Mechanism: float codegen. VS2013 emits `fstp [ebp-8]; movss xmm0,[ebp-8]; subss xmm0,[ebp-4];
  mulss xmm0,[ebp+0Ch]`, which rounds to float after the subtract and after the multiply. cl 19.44
  emits `call GetHeight; fsub dword ptr [ebp-4]; fmul dword ptr [ebp+0Ch]; fstp dword ptr [ebp-8]`,
  which keeps the difference on the x87 stack at 53-bit precision and rounds once after the
  multiply. `/fp:precise` allows both. The other three branches are SSE on both builds, so 1
  terrain sample in 4 is affected (even row, `fFracZ <= fFracX`). At dag1c_ctf turn 202 every
  input is bit-identical and the height is `3DA8635E` on VS2013 against `3DA8635C` on v143. That
  height feeds the collision depth, then `mOverlap`, then the soldier's `position.y` snap, which
  is why most C1 missions first split in one soldier's `position.y` (1 ulp of a 0.125-1 height,
  2^-26 to 2^-24 absolute).
- Class: toolchain-inherent. The code is well-defined float arithmetic.
- D-15 step that reproduced it: step 4, per-TU toolchain. `RedTerrain.cpp` compiled by VS2013 with
  the v120 flags and linked into the v143 exe gives `3DA8635E`, and dag1c_ctf then equals VS2013
  over all 1600 turns. The steps before it did not hit: the code-bug screen found no
  out-of-range index, no NaN or out-of-range conversion and no pointer-dependent order;
  `<cmath>`: the path calls only `RedTerrain::GetHeight` and inline `cvttss2si`/`cvtdq2ps`, so
  there is no math call to swap; `/fp`: the compile flags are the same, and `/fp:strict` on
  `RedTerrain.cpp` alone keeps the x87 pair and the split at 202; CRT: the path calls no libm or
  helper symbol, and a probe built against msvcr120 and against the UCRT prints the same bits.
- Neutralising experiment: NEUTRAL-C1 forces the line 241 subtract and multiply to SSE single
  with `_mm_sub_ss`/`_mm_mul_ss`, and dag1c_ctf then equals VS2013 over 1600 turns (first split
  202, then none).
- Status: neutralised in the main tree in 20-12 (a381f01) with the same `_mm_sub_ss`/`_mm_mul_ss`
  form. T1 `terrain interpolated height rounds each step to float` pins it, and guard-revert rows
  SIMFIX-C1 (T1) and SIMFIX-C1S (the `digest-dag1c_ctf` smoke row) go RED when it is reverted.
  Fix check: every C1 mission and hot1g_con equals VS2013 in every traced turn on the fixed
  build, with the VS2013 final line.
- Phase 20 effect: the first split of 13 missions (dag1c_con, dag1c_ctf, dag1g_con, dag1g_ctf,
  end1g_1flag, end1g_con, hot1g_xl, kas2c_c, kas2c_xl, hot1g_hunt, kas2c_con, kas2g_con,
  kas2g_xl), the hot1g_con blip, and the later splits of kas2c_uber (1705) and pol1c_con (5909).

### C2

- Function: `AIDifficulty::GetTargetBubbleShrinkRate` in `src/Battlefront2/Source/AIDifficulty.cpp`
  (TU `AIDifficulty.cpp`) line 206, `// FUNCTION: SWBF2 0x00438ed0`. Its result is consumed by
  `fShrink = GetTargetBubbleShrinkRate(..) * dt` in `Controller::UpdateTargetBubbleSize`,
  `src/Battlefront2/Source/Controller.cpp` line 219.
- Mechanism: float codegen. VS2013 emits `divss; subss xmm0; movss [ebp+8]; fld [ebp+8]` and
  returns a value rounded to float. Under `/fp:precise`, cl 19.44 emits `call GetFloatSkill; fsubr
  dword ptr [__real@bdcccccd]; ret`, so `-0.1f - skill` comes back in ST0 at 53-bit precision
  without the rounding at return that the `/fp:precise` documentation describes. The caller's
  `fmul dword ptr [dt]` is the same instruction on both builds and rounds the product once. With
  identical inputs (skill 11, dt `3D0852D8`) `fShrink` is `BCAAC366` on VS2013 against `BCAAC365`
  on v143. From turn 5 every AI controller's `mAvailableBubbleCharge` is 1 ulp off. That charge
  drives the target-bubble random walk, then the aim bubble, the heading, the turn input, the
  soldier rotation (geo1c_con turn 1623) and the velocity (turn 1629), which is why most C2
  missions first split in a velocity with an equal position. Later the AI's Sim random draws
  differ (geo1c_con from turn 2911).
- Class: toolchain-inherent. The code is well-defined.
- D-15 step that reproduced it: step 2, `/fp`. `/fp:strict` on `AIDifficulty.cpp` alone makes v143
  round before the return (`subss/divss/mulss/subss; movss [ebp+8]; fld`), gives `BCAAC366`, and
  geo1c_con then equals VS2013 in all 9014 turns, final score included. The steps before it did not
  hit: the code-bug screen found integer skill from constant tables, no pointer order and no NaN
  or out-of-range conversion; `<cmath>`: there is no math call or intrinsic on either path.
- Neutralising experiment: NEUTRAL-C2 sets `/fp:strict` on `AIDifficulty.cpp` only, and geo1c_con
  equals VS2013 to game over (first split 1629, then none). It leaves dag1c_ctf at 202, so C1 and
  C2 are independent.
- Status: neutralised in the main tree in 20-12 (a381f01) with `<FloatingPointModel>Strict</FloatingPointModel>`
  on the `AIDifficulty.cpp` item in `BattlefrontII.vcxproj`. T1 `ai difficulty compiles with fp
  strict` pins it, and guard-revert rows SIMFIX-C2 (T1) and SIMFIX-C2S (the `digest-kas2c_uber`
  smoke row) go RED when it is reverted. Because cl rejects `/fp:strict` together with
  `/fp:contract` (D8016), guard-revert row FLAG-FPCONTRACT now goes RED by build failure on the
  first `/fp:strict` TU. Fix check: every C2 mission equals VS2013 in every traced turn on the
  fixed build, with the VS2013 final line.
- Phase 20 effect: the first split of 16 missions (cor1g_con, kas2c_uber, myg1c_ctf, geo1c_con,
  geo1c_xl, kam1g_con, mus1c_con, nab2c_con, pol1c_con, pol1g_con, spa3c_ass, spa6c_ass,
  spa7c_ass, spa9g_ass, tat2g_eli, yav1c_con), and the later splits of dag1c_con (2862), dag1g_con
  (7440), end1g_con (7298) and kas2g_xl (8152).

### C3

- Function: `WeaponCannon::Fire` in `src/Battlefront2/Source/WeaponCannon.cpp` (TU
  `WeaponCannon.cpp`), `// FUNCTION: SWBF2 0x0067f320`, line 750: the yaw spread,
  `fYaw += (PblRandom::GetSim().GetFloat() - PblRandom::GetSim().GetFloat()) * GetYawSpread()`.
- Mechanism: float codegen, in the opposite direction from C1 and C2: here VS2013 keeps the x87
  precision and v143 rounds. `GetYawSpread` returns `mStanceSpread + fKick + fYaw` in ST0 at 53-bit
  precision on both builds (here 0 + 0.04247 + 0.004363, which needs 28 mantissa bits). VS2013
  emits `call GetYawSpread; fmul dword [r3-r4]; fadd dword [pattern yaw]; fstp dword [fYaw]`, so it
  multiplies and adds the unrounded return value on the x87 stack and rounds to float once. Under
  `/fp:precise`, cl 19.44 emits `call GetYawSpread; fstp dword [ebp-0B4h]; movss xmm2,[ebp-0B4h];
  mulss xmm2,xmm1; addss xmm2,[pattern yaw]`, which rounds the return value to float (`3D3FD5D5`)
  before an SSE single multiply and add. With every input bit-identical, `fYaw` is `BD040B0D` on
  VS2013 against `BD040B0E` on v143. One ulp of yaw gives one ulp of `sin(yaw)` and of the fire
  direction's x (`3ED036D4` against `3ED036D5`), then the bullet velocity, the hit direction in
  `OrdnanceBullet::Collide` and `EntitySoldier::DirectionKill`'s `mVelocity.x += vDir.x * 5.0f` on
  the killed soldier. The pitch line has the VS2013 shape on both builds. Only an AI shot whose
  yaw spread sum is not a float, followed by a kill, shows it in the digest, which is why it
  appeared once in 20226 turns.
- Class: toolchain-inherent. The code is well-defined float arithmetic.
- D-15 step that reproduced it: step 2, `/fp`. `<FloatingPointModel>Strict</FloatingPointModel>`
  on `WeaponCannon.cpp` alone makes v143 emit the VS2013 yaw sequence and gives `BD040B0D`. The
  steps before it did not hit: the code-bug screen found member floats with constructor-set
  values, Sim `PblRandom` draws evaluated in the same order with equal values, no pointer-dependent
  order and no NaN or out-of-range conversion; `<cmath>`: `cos` and `sin` take `(double)float`
  arguments and compile to the same `__libm_sse2_cos_precise`/`__libm_sse2_sin_precise` calls on
  both builds, and their argument already differs before the call. A probe that recomputes Fire's
  arithmetic both ways gives the VS2013 bits for the x87 sequence and the v143 bits for the SSE
  sequence under both msvcr120 and the UCRT.
- Side effect of `/fp:strict`: Fire's `cos` and `sin` become UCRT `_cos`/`_sin` calls. A sweep of
  8000002 float inputs (4000001 on [-pi, pi] and 4000001 random magnitudes from 2^-15 to 2^16)
  gives the same result hash (`319C91DB`) for VS2013 `/fp:precise` (msvcr120 `__libm_sse2_*`),
  v143 `/fp:precise` (UCRT `__libm_sse2_*`) and v143 `/fp:strict` (UCRT `_cos`/`_sin`), so the
  routine change does not change any `cos` or `sin` result.
- Neutralising experiment: NEUTRAL-C3 adds the `/fp:strict` item on `WeaponCannon.cpp` to a v143
  build that already has the C1, C2 and C-OOB-SIDE fixes, and geo1c_xl then equals VS2013 over all
  20226 turns to game over, `state=` and `rand=`, with the VS2013 final line (split 19926, then
  none).
- Status: neutralised in the main tree in 20-14 (a2c980c) with
  `<FloatingPointModel>Strict</FloatingPointModel>` on the `WeaponCannon.cpp` item in
  `BattlefrontII.vcxproj`. T1 `weapon cannon fire compiles with fp strict` pins it, and
  guard-revert row SIMFIX-C3 goes RED when it is reverted. Only that T1 source pin and SIMFIX-C3
  guard C3: no turn-1500 digest row can catch a split at turn 19926. Fix check: all 30 proof traces
  (the 29 drift missions and hot1g_con) equal VS2013 in every traced turn on the fixed build,
  `state=` and `rand=`, and every game-over mission has the VS2013 final line; geo1c_xl is equal
  over all 20226 turns.
- Phase 20 effect: none on any first split or final score. With C1 and C2 neutralised it was the
  geo1c_xl turn 19926 (char 119, team 2, `vel.x` `40D74737` against `40D74738`, 1 ulp), equal
  again from turn 19927.

### C-OOB-SIDE

- Function: `GameLoop::Update` in `src/Battlefront2/Source/GameLoop.cpp` (TU `GameLoop.cpp`),
  `// FUNCTION: SWBF2 0x005323e0`, line 1572 on the Phase 20 build: the `finalscore.log` write,
  `sSideNamePrefixes[pTeam->mSide]`.
- Mechanism: out-of-bounds read. `Team::TranslateTeamName` (`Team.cpp:291`) returns side_HEROES = 6
  and side_VILLAINS = 7 for the hero teams, and `sSideNamePrefixes` has 6 entries of `char[4]`.
  Index 6 and 7 read the 8 bytes after the table. In the VS2013 exe those are the next float
  constants (`00 00 A0 40`, `00 00 F0 41`), which print as empty names. In the v143 exe they are an
  absolute pointer that ASLR relocates at load, so the text changes from run to run (`?j?` and
  `\D?` in the Phase 20 table) and can run on into the next entry.
- Class: code bug. It does not touch the sim: the score, points, mission time and every traced
  turn are the same with and without the fix.
- D-15 step: none, it is not a sim cause. It was found in the 20-11 attribution as the only
  difference left in tat2g_eli's final line under NEUTRAL-C12, and the bytes behind index 6 and 7
  were read from each exe at the map address of `sSideNamePrefixes`.
- Neutralising experiment: the code-bug rule, a VS2013-toolset build plus a fix against a v143
  build plus the same fix (entries `Hro` and `Vil` for sides 6 and 7), gives identical tat2g_eli
  lines, `L=Hro 32u@11P -1r 178p W=Vil 32u@14 -1r 180p`.
- Status: fixed in 20-12 (c539eee). `SideNamePrefix` in `src/Battlefront2/Source/SideNames.h`
  returns the table entry for sides 0-5 and an empty name otherwise, which is the VS2013 text.
  T1 `side name prefix is bounded` and `final score uses the bounded side name`, guard-revert row
  SIDENAME-01. Fix check: tat2g_eli's final line equals VS2013's,
  `tat2g_eli 1371s L= 32u@11P -1r 178p W= 32u@14 -1r 180p`.

## Factory

All four `factory-<m>.txt` logs are byte-identical to VS2013. D-16 passed: Factory creation order,
and so `mnetIndex`, is unchanged.

## Initializers

| | VS2013 | v143 |
|-|--------|------|
| in-range initializers (`initializers.txt`) | 1681 | 944 |
| `??__E` dynamic initializers (`initializers-di.txt`) | 1695 | 949 |
| discarded (`initializers-discarded.txt`) | 17 | 8 |
| objs with initializers | 405 | 331 |

328 objs have initializers on both sides, and their order is identical (0 inversions). 75 game
objs no longer have a dynamic initializer because v143 constant-initializes them. One obj gains
one: `netDrag.obj` `bandwidthQueue` (`netPktGroup[3]`, the dev-only drag simulator). The CRT
sentinel objs change name (`cinitexe.obj`, `crtexew.obj` become `initializers.obj`,
`exe_winmain.obj`). Factory order is unaffected, as the identical factory logs show.

## Float-parse corpora

`odf.txt` (784 retail tokens): 0 of 784 differ. D-17 was not triggered.

`edge.txt`: 12 of 61 lines differ.

| Family | Lines | v143 result |
|--------|-------|-------------|
| long mantissa (`1.0000000596046447753906250001`) | 1 | `sscanf` rounds to `3F800001` |
| denormal (`1e-45`) | 1 | `sscanf` gives `00000001` |
| hex (`0x1p3`, `0x10`) | 2 | parsed as hex, 8 and 16 |
| dangling exponent (`1e`, `1e+`) | 2 | `sscanf` ret -1, target untouched |
| `1.5e-x` | 1 | `sscanf` ret 0, target untouched |
| nan/inf (`nan`, `nAn`, `inf`, `-inf`, `infinity`) | 5 | ret 1, non-finite |

On the game path the nan/inf and dangling-exponent tokens go through `OdfValue::Sanitize`
(TOOL-07), which gives the VS2013 results. The hex, long-mantissa, denormal and `1.5e-x` deltas
are accepted UCRT differences; every retail token in `odf.txt` parses identically.

## Math corpus

179 of 10636 lines differ, in Release and Debug.

| Form | Lines | Cause |
|------|-------|-------|
| `atan2.f`, `atan2.d` | 4, 4 | `atan2(±inf, ±inf)` is ±π/4 or ±3π/4 instead of nan |
| `fmod.f`, `fmod.d` | 25, 46 | `fmod(-0, y)` is -0 instead of +0; `fmod(x, ±inf)` is x instead of nan |
| `pow.ff`, `pow.dd` | 3, 3 | `pow(1, nan)` and `pow(nan, ±0)` are 1 |
| `pow.fi`, `pow.di` | 66, 28 | C++11 `pow(T, int)` promotes to double `pow`; VS2013 used `_Pow_int`, up to 1 ulp |
| sin, cos, tan, asin, acos, atan, sqrt, exp, log, log10, floor, ceil | 0 | |

The D-15 bisect proved no CRT function load-bearing for the sim. The CRT step did not hit for C1:
its path calls no libm or helper symbol, and the probe gives the same bits on msvcr120 and on the
UCRT. C2 and C3 were reproduced at the `/fp` step, before the CRT step, and the `_cos`/`_sin`
calls that the C3 fix brings in give the same results as the `__libm_sse2_*` routines (see `### C3`).
There is no `pow(float, int)` call site in game code. The sim drift on the Phase 20 build is the
codegen causes C1, C2 and C3 under `## Causes`, and with all three neutralised the fixed build
equals VS2013 on every attributed mission.

`math corpus x87 pc24`: the same 179 lines. Under `_PC_24`, Debug reports 273 `sqrt.d`/`atan.d`
lines following x87 precision control and Release reports 0, the same as VS2013.

## -Smoke All vs VS2013

`tests\run.ps1 -NoBuild -Smoke All` on the fixed v143 Release build: 169 PASS, 0 FAIL. The 150
`smoke:<mission>` rows match `tests/baseline/vs2013/smoke-all.txt` exactly, mission by mission and
class by class (Pass or TimeOut): 150 passing on VS2013, 150 passing on v143, and no mission
changed result. The Phase 20 build gave the same 150 rows. The other rows in `smoke-all.txt`
differ only because the v143 hardening block has the four `flag-*` checks (169 PASS instead of
165) and a different ASLR base.

`smoke-all-digests.txt` has the turn-1500 digest and the final-score line of every mission. On
the fixed build every row equals VS2013 except `dedicated`. On the Phase 20 build 137 of 150
`state=` values were identical to VS2013. The 13 that differed:

| Mission | First split turn | Turns that differ (of 1600) | First `rand=` split | First field | Cause |
|---------|------------------|-----------------------------|---------------------|-------------|-------|
| cor1g_con | 1463 | 134 | none | char 5 `vel.x`, 8 ulp | C2 |
| dag1c_con | 231 | 782 | none | char 4 `pos.y`, 1 ulp | C1, then C2 from 2862 |
| dag1c_ctf | 202 | 912 | none | char 25 `pos.y`, -64 ulp | C1 |
| dag1g_con | 681 | 920 | none | char 43 `pos.y`, -2 ulp | C1, then C2 from 7440 |
| dag1g_ctf | 293 | 1277 | none | char 36 `pos.y`, -256 ulp | C1 |
| end1g_1flag | 394 | 1195 | none | char 14 `pos.y`, 1 ulp | C1 |
| end1g_con | 683 | 348 | none | char 13 `pos.y`, -2 ulp | C1, then C2 from 7298 |
| hot1g_xl | 764 | 837 | none | char 8 `pos.y`, 4 ulp | C1 |
| kas2c_c | 1067 | 479 | none | char 53 `pos.y`, -1 ulp | C1 |
| kas2c_uber | 636 | 923 | none | char 7 `vel.x/y/z`, -2/-3/2 ulp | C2, then C1 from 1705 |
| kas2c_xl | 1065 | 532 | 1393 | char 163 `vel.y`, -1 ulp | C1 |
| myg1c_ctf | 1125 | 434 | none | char 14 `vel.y/z`, -2/-1 ulp | C2 |
| dedicated | | | | | not deterministic, see below |

Method: the archived VS2013 Release exe (`vs2013-release-1168dd4.zip`, the build that produced
`tests/baseline/vs2013/smoke-all.txt`) ran each mission twice with `-digesttrace 1600`, and the
Phase 20 v143 exe ran it once, on the same staged data. The two VS2013 runs are identical on every
turn (split 0) and their final digests equal the VS2013 table. Two full Phase 20 `-Smoke All` runs
gave the
same 12 values. geo1c_con, the control, is identical on all three. So both builds are
deterministic and the drift is between toolchains.

Attribution: the first field is the first differing field from field-level logging of a
VS2013-toolset build and a v143 build of the same tree over `[split-3, split+3]`, each checked
against its own toolchain's trace (char = character index, ulp = v143 minus VS2013). The cause is
from a neutral-build experiment: the NEUTRAL-C1 and NEUTRAL-C2 v143 builds, each with one cause
neutralised, and NEUTRAL-C12 with both, ran every mission to game over (TimeOut missions to turn
1600) against the VS2013 run. A mission belongs to the cause whose own neutral build moves its
first split later or removes it while the other cause's build leaves it at the same turn. No
mission was moved by both single-cause builds or by neither. Under NEUTRAL-C12 every mission
equals VS2013 in every traced turn, except tat2g_eli's side-name text (C-OOB-SIDE) and one turn
of geo1c_xl (C3, see `## Final score vs VS2013`). A later split that remains under the attributing build belongs to the other cause,
as the table shows. `rand=` stays identical to turn 1600 in 11 of the 12. In kas2c_xl the drift
reaches a random draw at turn 1393. That is C1: NEUTRAL-C1 removes both its `state=` split at
1065 and its `rand=` split at 1393, and NEUTRAL-C2 leaves both.

`dedicated` is a host and a client over loopback and is not deterministic: two v143 runs gave
`0xD9DDDD36` and `0x256999AB`. It passes on both toolchains.

## Final score vs VS2013

`finalscore.log` is written at game over: after 309 s for the CON missions and up to 1638 s for
assault (spa6c_ass), long after the turn-1500 digest. A split after turn 1600 does not show in the
digest or in a 1600-turn trace but still changes the score. On the Phase 20 build the final-score
columns of 24 missions differed from VS2013. Their first split turn and first field come from a
`-digesttrace` run to game over on both exes and the field window above. The cause is the
attribution experiment above. On the fixed build all 24 final lines equal VS2013.

| Mission | VS2013 | v143 (Phase 20) | Winner | First split turn | First field | Cause | Status |
|---------|--------|-----------------|--------|------------------|-------------|-------|--------|
| cor1g_con | 309s W=All 32u@9P 78r, L=Imp 32u@16 43r | 309s L=All 32u@11P 49r, W=Imp 32u@14 68r | flip | 1463 | char 5 `vel.x`, 8 ulp | C2 | now equal after fix |
| dag1c_con | 309s L=Rep 32u@11P 68r, W=CIS 32u@14 85r | 309s L=Rep 32u@11P 72r, W=CIS 32u@14 92r | same | 231 | char 4 `pos.y`, 1 ulp | C1, then C2 from 2862 | now equal after fix |
| dag1g_con | 309s L=All 32u@14P 40r, W=Imp 32u@11 89r | 309s L=All 32u@14P 50r, W=Imp 32u@11 99r | same | 681 | char 43 `pos.y`, -2 ulp | C1, then C2 from 7440 | now equal after fix |
| end1g_con | 309s W=All 29u@8P 104r, L=Imp 29u@17 64r | 309s W=All 29u@7P 109r, L=Imp 29u@18 55r | same | 683 | char 13 `pos.y`, -2 ulp | C1, then C2 from 7298 | now equal after fix |
| geo1c_con | 309s L=Rep 28u@15P 15r, W=CIS 28u@10 95r | 309s L=Rep 28u@15P 36r, W=CIS 28u@10 101r | same | 1629 | char 11 `vel.x/y/z`, 1/-1/-3 ulp | C2 | now equal after fix |
| geo1c_xl | 682s L=Rep 80u@11P 320p, W=CIS 80u@14 350p | 694s L=Rep 80u@11P 316p, W=CIS 80u@14 350p | same | 2261 | char 10 `vel.y/z`, 1/6 ulp | C2, then C3 at 19926 | now equal after fix; the turn-19926 residual is C3, fixed |
| hot1g_hunt | 231s L=Loc 32u@11P 28p, W=All 32u@14 54p | 231s L=Loc 32u@11P 33p, W=All 32u@14 51p | same | 731 | char 39 `pos.y`, 1 ulp | C1 | now equal after fix |
| hot1g_xl | 498s L=Imp 120u@11P 220p, W=All 120u@14 350p | 472s L=Imp 120u@11P 186p, W=All 120u@14 350p | same | 764 | char 8 `pos.y`, 4 ulp | C1 | now equal after fix |
| kam1g_con | 309s W=All 32u@10P 113r, L=Imp 32u@15 85r | 309s W=All 32u@11P 108r, L=Imp 32u@14 89r | same | 2154 | char 6 `vel.x/z`, -1/-2 ulp | C2 | now equal after fix |
| kas2c_con | 309s W=Rep 29u@11P 102r, W=CIS 29u@14 102r | 309s W=Rep 29u@11P 109r, L=CIS 29u@14 99r | flip | 5542 | char 27 `pos.z`, -1 ulp | C1 | now equal after fix |
| kas2c_uber | 309s W=Rep 50u@11P 7r, L=CIS 250u@14 3r | 309s W=Rep 50u@10P 11r, L=CIS 250u@15 3r | same | 636 | char 7 `vel.x/y/z`, -2/-3/2 ulp | C2, then C1 from 1705 | now equal after fix |
| kas2c_xl | 419s L=Rep 160u@11P 327p, W=CIS 160u@14 354p | 417s L=Rep 160u@11P 335p, W=CIS 160u@14 352p | same | 1065 | char 163 `vel.y`, -1 ulp | C1 | now equal after fix |
| kas2g_con | 309s W=All 29u@11P 122r, L=Imp 29u@14 112r | 309s W=All 29u@11P 119r, L=Imp 29u@14 111r | same | 3685 | chars 31 and 43 `vel.y`, -1 ulp | C1 | now equal after fix |
| kas2g_xl | 377s W=All 160u@11P 351p, L=Imp 160u@14 332p | 393s W=All 160u@11P 351p, L=Imp 160u@14 315p | same | 3000 | char 299 `pos.y`, -1 ulp | C1, then C2 from 8152 | now equal after fix |
| mus1c_con | 309s L=Rep 32u@15P 22r, W=CIS 32u@10 78r | 309s L=Rep 32u@12P 37r, W=CIS 32u@13 73r | same | 1080 | char 16 `vel.x`, 1 ulp | C2 | now equal after fix |
| nab2c_con | 309s W=CIS 32u@7P 85r, L=Rep 32u@18 38r | 309s W=CIS 32u@7P 89r, L=Rep 32u@18 42r | same | 5206 | char 16 `vel.x`, 1 ulp | C2 | now equal after fix |
| pol1c_con | 309s W=CIS 32u@8P 95r, L=Rep 32u@17 55r | 309s W=CIS 32u@7P 90r, L=Rep 32u@18 38r | same | 3091 | char 3 `vel.x/y/z`, 1/1/1 ulp | C2, then C1 from 5909 | now equal after fix |
| pol1g_con | 309s W=All 32u@7P 99r, L=Imp 32u@18 37r | 309s W=All 32u@7P 93r, L=Imp 32u@18 48r | same | 5605 | char 31 `vel.y/z`, -3/2 ulp | C2 | now equal after fix |
| spa3c_ass | 1317s L=Rep 32u@11P 171p, W=CIS 32u@14 196p | 1337s L=Rep 32u@11P 178p, W=CIS 32u@14 189p | same | 2445 | char 33 `hp`, 1 ulp | C2 | now equal after fix |
| spa6c_ass | 1638s L=Rep 32u@15P 86p, W=CIS 32u@10 180p | 1580s L=Rep 32u@15P 80p, W=CIS 32u@10 189p | same | 12022 (`rand=` 12008) | char 25 `hp`, a different damage roll | C2 | now equal after fix |
| spa7c_ass | 1024s L=Rep 32u@15P 65p, W=CIS 32u@10 180p | 1074s L=Rep 32u@15P 70p, W=CIS 32u@10 180p | same | 21452 | char 14 `vel.x/z`, -6/-5 ulp | C2 | now equal after fix |
| spa9g_ass | 1134s W=Imp 32u@11P 181p, L=All 32u@14 173p | 1098s L=Imp 32u@11P 173p, W=All 32u@14 185p | flip | 5523 | char 58 `vel.z`, -1 ulp | C2 | now equal after fix |
| tat2g_eli | 1371s L= 32u@11P 178p, W= 32u@14 180p | 1371s L=`?j?` 32u@11P 178p, W=`\D?` 32u@14 180p | same | 20534 | char 27 `vel.x`, -1 ulp; side-name text | C2 (sim), C-OOB-SIDE (text) | now equal after fix |
| yav1c_con | 309s W=CIS 32u@7P 106r, L=Rep 32u@18 33r | 309s W=CIS 32u@7P 106r, L=Rep 32u@18 40r | same | 3410 | chars 4 and 15 `vel.x/z`, 3/1 ulp | C2 | now equal after fix |

The columns are the side, units, ticket or point counts of each team; the `0p` and `-1r` columns
that every row shares are left out. cor1g_con, kas2c_con and spa9g_ass are the winner flips:
cor1g_con and spa9g_ass under C2, kas2c_con (W/W on VS2013, W/L on v143) under C1. In spa6c_ass
the drift reaches a Sim random draw at turn 12008 before any hashed field differs, and the
different draw shows at 12022 as a different damage amount on char 25. tat2g_eli's sim split at
20534 changes no column; its final line differed only in the side-name text, which is
C-OOB-SIDE. The other 23 differences are score, unit, point or time changes that follow from the
split.

geo1c_xl: with C1 and C2 neutralised, the `-digesttrace` run to game over still differed from
VS2013 at one turn, 19926 of 20226, in one field (char 119, team 2, `vel.x` `40D74737` vs
`40D74738`, 1 ulp). That residual is C3, and it is fixed. The fixed build equals VS2013 on every
traced turn of the 30 proof traces (the 29 drift missions and hot1g_con, to game over or to turn
1600 for the TimeOut missions and hot1g_con), geo1c_xl over all 20226 turns.

## Policy

The v143 files in `tests/corpus/` and this directory are the reference from now on. The digest
must stay identical; a change needs a new written cause.
