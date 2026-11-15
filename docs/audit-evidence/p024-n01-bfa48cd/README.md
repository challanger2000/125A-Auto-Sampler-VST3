# P024 N01 native Win32 Recorder failure-injection evidence

**Exactly executed source:** `bfa48cd3555d64950aa3b7a0545a17ef5a5ae2ea` (PR #2 / develop/p001-autosampler-foundation).
The evidence-only commit containing this directory has NO product/test/workflow changes.
All logs are literal GitHub Actions job transcripts copied with tool access.
They are **developer CI evidence**, not new independent Codex Windows execution.

## Windows execution

- [Windows VST3 run 43661623198](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43661623198), job 132652014955: **SUCCESS**
- [Windows core run 43661623242](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43661623242), job 132652014919: **SUCCESS**
- Steinberg Validator: **94/94**, no failures.
- The native test drives the real dual-component VST3 recorder, finishes eight takes, injects `std::bad_alloc` **immediately after real RUN-REPORT close** (before publication), waits for cleanup, inspects actual filesystem and DAW project state. All eight WAVs must remain, with zero .125A, zero partial/final report and zero authorized bank.
- The test-only `A125_AuditSkipReportFailureCleanup` mutant is run in another Windows process. The identical assertions **must** fail with `N01 retained published archive or partial report after failed close`. CI checks nonzero exit and exact diagnostic.

### Source-verifiable results

```text
2026-11-15T01:10:33.7206768Z   Result: 94 tests passed, 0 tests failed
2026-11-15T01:10:47.3746116Z   Result: 94 tests passed, 0 tests failed
2026-11-15T01:11:01.4099843Z Result: 94 tests passed, 0 tests failed
2026-11-15T01:12:05.5235733Z [PASS] S02 rejected-newer-request mutation killed by full state + PCM assertions
2026-11-15T01:12:05.6927961Z [PASS] F01 postpublication allocation mutation killed
2026-11-15T01:12:14.1934580Z [PASS] REAL VSTGUI main instrument editor: 19 parameters, Win32 HWND attach, timer and clean detach\n[PASS] REAL VSTGUI recorder status editor: 0 parameters, Win32 HWND attach, timer and clean detach\n[PASS] P021 GATE->ONE SHOT: old project and two new-state recalls preserve ON\n[PASS] N01 actual Windows Recorder: post-close bad_alloc rolled back .125A + partial report; 8 WAVs intact; no authorized DAW bank\n[PASS] N01 actual Windows Recorder post-close allocation mutant killed
2026-11-15T01:13:20.7734854Z Artifact 125A-AutoSampler-P22-Independent-MultiClass-Inventory.zip successfully finalized. Artifact ID 14140816158
2026-11-15T01:13:23.2669585Z Artifact 125A-AutoSampler-P024-Win64.zip successfully finalized. Artifact ID 14140274143
```

## Shipping artifact, not QA-DLL

- [Original Windows artifact 14140274143](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43661623198/artifacts/14140274143)
- SHA-256 (downloaded ZIP bytes): `64b34930f63c72e2cc7d126b89d9909313a07672ab00a7a6500680a6a3682573`
- Six ZIP files, all under `AutosamplerVST3.vst3/`.
- PE export table read from actual shipped `Contents/x86_64-win/AutosamplerVST3.vst3` with GNU objdump: `ExitDll`, `GetPluginFactory`, `InitDll` (no `A125_Audit*` exports).

## Limits

Only the exact N01 fault-injection point after successful report close is independently deterministically triggered by this developer QA-DLL. It does not qualify all host, OOM, file-system, teardown or disk-full paths. The independent P024 audit decision remains separate; this document does **not** assert GO for P025.
