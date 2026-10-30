# v143 baseline manifest

## Build

| Key | Value |
|-----|-------|
| toolset | v143 |
| cl | 19.44.35229 |
| MSVC tools version | 14.44.35207 |
| ucrtbase.dll | 10.0.26100.9711 |
| vcruntime140.dll | 14.51.36247.0 |
| configuration | Release |
| commit | eff28502bae0530cde8ce09c734c37cd1dc4f8ef |
| tracked tree clean | yes |

Differences from `tests/baseline/vs2013/` and their causes are in [CAUSES.md](CAUSES.md).

## Text artifacts

| File | SHA-256 | Produced by |
|------|---------|-------------|
| `CAUSES.md` | 012401A6E72A970B8194226E67F578910CEB03C251AE6CA5B6D44287342945C1 | written by hand from the comparison against `tests/baseline/vs2013/` |
| `smoke-all.txt` | 0C44E698C621B0852ADC8E089744C27DDBCBF1BDFF71B2C2BC6C6D8C0BC26352 | `capture-vs2013.ps1 -Tables` from `run.ps1 -Smoke All` |
| `smoke-all-digests.txt` | 745F41BA50A0A720C6D172B6A69AFBF05BAE007A8EABEAA420C784FBA9B63393 | `capture-vs2013.ps1 -Tables` from `run.ps1 -Smoke All` |
| `hardening.txt` | 79E6FB275A17F199CFE178DC989447A1CB3A97870486BE10F5A1B5B31C7EDC7F | `capture-vs2013.ps1 -Tables` from `run.ps1 -Hardening` |
| `guard-revert.txt` | D2AFAF3A7A490DA59A2ECD1EE133BFD1B1FB0AAB2058A02E7A1D29A70C87FBFA | `capture-vs2013.ps1 -Tables` from `guard-revert.ps1` |
| `digests-debug.txt` | D285E11C8F109D5C586D948E8F2A653B2EDD89D7367838A74D6EC10BA3B1DB44 | `capture-vs2013.ps1 -DebugDigests` from `run.ps1 -Smoke Twice -SmokeConfiguration Debug` |
| `digests.txt` | A6C3F1C777CDB1F84A3C932A6932EA5721C4B8898E72A49024C606866EC76E3D | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `digests-hog.txt` | A6C3F1C777CDB1F84A3C932A6932EA5721C4B8898E72A49024C606866EC76E3D | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `digest-trace-geo1c_con.txt` | A3EA86CB6038FAB58178D3549E67E8EF8675347EE1DFD194233F280A6A0EF6F5 | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `digest-trace-hot1g_con.txt` | C22AA71DB3546F113343377DFA6786B14E19D82B9817F510E54EC74CBF67C594 | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `digest-trace-nab2g_c.txt` | F7B2B8670597008821CA60FE2CFF9C7F51C95D2FE42E51B7DB8D5C96953FCBDA | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `digest-trace-spa3c_ass.txt` | 47EB12CE384D249E5FC947E37DC8C5C13F6F89A95A1DD899B687503434AC8186 | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `factory-geo1c_con.txt` | 0C3C380156342E1C492092575630152541B648FF038F8B6285A840C6F73D9C7A | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `factory-hot1g_con.txt` | 21739D89813BFA37EC08B53900C0EDE5AD79CA96D8DFDEBAEDE5692127F3E5D6 | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `factory-nab2g_c.txt` | 2192336505D7A03D0ABD400927A962AFB6EF09358AB6E572657AC7A4EC0C94E7 | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `factory-spa3c_ass.txt` | 0FDB0B17217BBC000279BBCD49AE05681C8ACB199306878FE0AA8E485714CABF | `capture-vs2013.ps1 -Collect -OutDir tests\baseline\v143` |
| `initializers.txt` | 32C8EA5C0CC23B85CCA7EEE44C766D2EE5FAB1967FC0008767064278F60999BD | `capture-vs2013.ps1 -Map -OutDir tests\baseline\v143` |
| `initializers-di.txt` | 61BF7A2C9A6AFD5C5D7B14C3AF6D41B8930E5C7EB93E1B9FD3CB406C1A85B1FE | `capture-vs2013.ps1 -Map -OutDir tests\baseline\v143` |
| `initializers-discarded.txt` | E7EB9AA0EF23D33DAC7A5732DBA8C8EA20DF8FA88F2EA85C95E669EF3D30CDB6 | `capture-vs2013.ps1 -Map -OutDir tests\baseline\v143` |

## Notes

- Release is the reference configuration. The binaries are not in the repo.
- Every file was captured or re-checked on commit eff2850, whose game exe is the a2c980c build with the 20-12 and 20-14 fixes (eff2850 changes only tests). Release exe SHA-256 `17A3A864356C23D254EA6A302DA0FF591A3013B0F5135A18F64E99E130A71885`. `smoke-all*.txt`, `hardening.txt` and `guard-revert.txt` were regenerated there; `smoke-all-digests.txt` came out byte-identical. `digest-trace-hot1g_con.txt` was regenerated on a381f01. `digests.txt`, `digests-hog.txt`, `digests-debug.txt`, the other three digest traces and the four factory logs were captured on 1b1cbca/dce2810, and all ten, with `digest-trace-hot1g_con.txt`, are byte-identical when re-captured on eff2850. The `initializers*.txt` files come from dce2810.
- `smoke-all-digests.txt` row `dedicated` is a real-time run, so its `state=`, `time=` and `rand=` vary between runs.
- `capture-vs2013.ps1 -Archive` and `-Verify` assume the VS2013 build identity and are not used for this directory.
