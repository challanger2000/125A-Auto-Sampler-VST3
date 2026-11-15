# P024 native Windows / GCC evidence (source SHA 39b8582)

**Immutable tested source commit:** `39b8582b818bc74325b1fb97a7de20125530d746` on PR #2, `develop/p001-autosampler-foundation`.

## Direct source and job links

- [Actual Windows-2022 VST3 job 132638942846](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43657409206): success
- [Windows-2022 core job 132638922754](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43657409197): success
- [Strict GCC14 core job 132638907360](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43657409156): success

The three adjacent `.log` files contain the **raw job transcripts returned from the GitHub Actions job-log API** on 15 November 2026, copied without editing. The original run numbers and job IDs enable provenance comparison if GitHub API access is available. This Git commit is an evidence mirror, **not** an independent test execution or proof of what a third party was allowed to inspect.

## Verifiable shipping archive

- [Original CI artifact 14139007283](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43657409206/artifacts/14139007283)
- GitHub artifact digest and independently calculated archive SHA-256: `c9b6d1d00f26ca81ded54e51fb8ab538711dbc3936b847541d6413228e14263b`.
- ZIP: 6 entries, all inside `AutosamplerVST3.vst3/`.
- Actual `Contents/x86_64-win/AutosamplerVST3.vst3` PE32+ export directory, parsed from downloaded binary: `ExitDll`, `GetPluginFactory`, `InitDll`. **No** `A125_Audit*` exports.
- Binary itself is hosted as a GitHub Actions artifact, **not** committed here.

## Native results copied verbatim from log

```text
2026-11-15T00:19:13.1239740Z   Result: 94 tests passed, 0 tests failed
2026-11-15T00:19:26.1240056Z   Result: 94 tests passed, 0 tests failed
2026-11-15T00:19:37.8503852Z Result: 94 tests passed, 0 tests failed
2026-11-15T00:19:45.6890803Z 100% tests passed, 0 tests failed out of 1
2026-11-15T00:20:14.0253277Z P010: project-restored custom folder, eight real VST3 WAVs
2026-11-15T00:20:37.7411655Z [PASS] S01 native Windows VST3 valid v2201 stream, rejected invalid name preserves all 18 params, archive identity, bank path, folder, name and PCM
2026-11-15T00:20:37.7412327Z [PASS] A03 native Windows concurrent modern setState: later project wins all state fields and PCM
2026-11-15T00:20:38.6170633Z [PASS] S02 rejected later restore variant 0 preserves fully accepted earlier state
2026-11-15T00:20:38.6171365Z [PASS] S02 rejected later restore variant 1 preserves fully accepted earlier state
2026-11-15T00:20:38.6172047Z [PASS] S02 rejected later restore variant 2 preserves fully accepted earlier state
2026-11-15T00:20:38.6172722Z [PASS] S02 rejected later restore variant 3 preserves fully accepted earlier state
2026-11-15T00:20:38.9594084Z [PASS] S02 rejected-newer-request mutation killed by full state + PCM assertions
2026-11-15T00:20:39.1095597Z [PASS] F01 postpublication allocation mutation killed
2026-11-15T00:21:22.5549312Z Independent Test Lab P22: two native VST3 classes inspected; P74 NOT SUPPORTED, not PASS
2026-11-15T00:21:23.2892728Z Artifact 125A-AutoSampler-P22-Independent-MultiClass-Inventory.zip successfully finalized. Artifact ID 14138747869
2026-11-15T00:21:24.4764189Z Artifact 125A-AutoSampler-P024-Win64.zip successfully finalized. Artifact ID 14139007283
```

The Core CTest suppresses output of passing individual test functions, but its source includes `testA06WindowsWavExclusivePublish` and `testA06WindowsReportPublicationFaults` in `main()`. Both execute on Windows under the passing core CTest. This is an actual Windows-core developer CI run, not an independent Test Lab function PASS.

## Limits that remain

S02/F01/S01 native VST3 positive and mutation checks, Steinberg 94/94 and A06 OS-level conflicts are developer regression evidence. Independent A01/A02/A05/A06 failure/teardown matrices and real Fender Studio Pro / physical ASIO testing remain separate. P22 is only a two-class inventory; the pinned generic P74 does not support the two-class bundle. **No P025 FULL-AUDIT GO asserted here.**
