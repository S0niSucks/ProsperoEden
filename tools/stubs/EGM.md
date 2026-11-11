# AGC import facade

## Overview

Link-time names used to construct firmware-module imports, not an AGC driver implementation. [Source](./libSceAgcDriver.c). `[libSceAgcDriver.c:sceAgcDriverSubmitDcb]`

## Interface

- Submission/rendering exports: `sceAgcDriverSubmitDcb`, `sceAgcDriverGetWaitRenderingPacketSizeInDwords`, `sceAgcDriverWaitUntilSafeForRendering`. `[libSceAgcDriver.c:sceAgcDriverSubmitDcb]`
- Ring exports: `sceAgcDriverSetTFRing`, `sceAgcDriverGetTFRing`. `[libSceAgcDriver.c:sceAgcDriverSetTFRing]`
- Off-chip parameter exports: `sceAgcDriverGetHsOffchipParam`, `sceAgcDriverSetHsOffchipParam`. `[libSceAgcDriver.c:sceAgcDriverGetHsOffchipParam]`
- The [stub builder](../build-agc-driver-stub.sh) produces the link-time shared object. `[../build-agc-driver-stub.sh:out]`

## Invariants

- All seven definitions take no arguments, return `int32_t(-1)`, and do not implement firmware behavior. `[libSceAgcDriver.c:sceAgcDriverSubmitDcb]`

## Side Effects

- None in the facade definitions; invoking the separate builder writes its shared object. `[libSceAgcDriver.c:sceAgcDriverSetTFRing]` `[../build-agc-driver-stub.sh:out]`

## Verification

- Repository-root `make test` is the host verification target; it fetches missing dependencies and builds the host core, not a PS5 driver qualification. Not executed for this documentation. `[Makefile:test]`
