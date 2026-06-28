# Plan: Origin tracking through standard-cell mapping (`&nf`)

Status: **draft / design** — follow-on to PR #487 (per-object origin tracking, `vOrigins`).
Branch: `origin-tracking-stdcell` (base: `origin-tracking-clean`).

## Goal

Make `\src` provenance survive **standard-cell** technology mapping so that an
ASIC flow (e.g. LibreLane → sky130/gf180) can label each mapped gate with the
RTL source location it came from.

PR #487 + the yosys `src-retention-y-ext` branch already deliver this for the
**FPGA / LUT** path: yosys writes XAIGER, ABC propagates `vOrigins` through the
LUT mapper (`&if`) and optimization, ABC writes the `"y"` extension, and yosys
applies `\src` to `$lut` cells. Validated end-to-end by
`tests/techmap/abc9_src_retention_full.sh` (100% of LUTs tagged).

The standard-cell path is **not** covered. This plan closes the ABC-side gap and
scopes the companion yosys work.

## Verified current state (why std-cell doesn't work today)

1. **`&nf` (the std-cell mapper, `src/aig/gia/giaNf.c`) is not origin-instrumented.**
   `grep -c Origin src/aig/gia/giaNf.c` = 0. PR #487 instrumented the LUT
   mappers (`giaIf.c`, `giaJf.c`, `giaLf.c`) and the optimization passes
   (`giaAig.c` dc2/dch, `giaMfs.c`, `giaBalAig.c`, `giaHash.c`, `giaDup.c` `&st`,
   …) but never the `&nf` cell mapper.

2. **The emission channel already exists.** `Gia_AigerWriteS` (`giaAiger.c:1876`)
   writes `vOrigins` as the variable-length `"y"` extension. If a mapped GIA
   carries `vOrigins`, `&write` emits them for free.

3. **The LUT instrumentation template is small and local.**
   `Gia_ManOriginsDupIf(pNew, p, pIfMan)` (`giaDup.c:458`) walks the `If_Man_t`
   objects, and for each source object `i` with origins, unions them into the
   mapped object `Abc_Lit2Var(pIfObj->iCopy)` of `pNew`. An `&nf` analogue needs
   the equivalent source→mapped-node correspondence from `Nf_Man_t`.

4. **LibreLane's std-cell flow uses the classic `abc` pass over BLIF, not XAIGER.**
   yosys `passes/techmap/abc.cc:1017`: `read_blif input.blif; <script>; write_blif output.blif`.
   The LibreLane strategies (`construct_abc_script.py`) do the actual mapping with
   `&get -n; &st; &dch; &nf; &put` (and `map`/`amap`), but the result returns to
   ABC's `Abc_Ntk_t` and is written as **BLIF**.
   **BLIF discards AIG object identity** — the very key the `"y"` extension uses
   to map mapped-objects back to source-objects. So origins cannot ride the
   existing classic-`abc`/BLIF channel even if `&nf` is instrumented.

### Consequence for phasing

- The ABC change (instrument `&nf` + emit via `&write`) is **necessary,
  minimal, and independently testable** — it does not depend on yosys.
- End-to-end LibreLane integration additionally requires the **mapping step to
  communicate over XAIGER** (object-id preserving), which is yosys-side work
  (`abc9` is currently LUT-only). That is the larger follow-on and is **out of
  scope for this ABC PR**, but documented below so the boundary is explicit.

## ABC PR scope (this branch) — minimal changes

Deliverable: **`vOrigins` survive `&nf` standard-cell mapping and are emitted by
`&write`'s `"y"` extension**, with ABC-level tests proving it.

### Changes

1. **`src/aig/gia/giaNf.c` — propagate origins through mapping.**
   `&nf` reads an input GIA `p` and produces a mapped GIA `pNew` (mapping stored
   in `pNew->vCellMapping`, freed at `giaNf.c:382`). At the point `pNew` is
   derived, call a new helper:

   ```c
   if ( p->vOrigins )
       Gia_ManOriginsDupNf( pNew, p, pNfMan );   // mirror of Gia_ManOriginsDupIf
   ```

   The exact derivation site and the source→mapped-node correspondence in
   `Nf_Man_t` must be pinned during implementation (candidate: the `iCopy`/
   object-copy bookkeeping used when `Nf_ManDeriveMapping` builds `pNew`). If
   `&nf` maps **in place** (annotating `p` with `vCellMapping`) rather than
   producing a fresh GIA, no remap is needed and `vOrigins` already align by
   object index — verify which case holds first.

2. **`src/aig/gia/giaDup.c` — add `Gia_ManOriginsDupNf`.**
   Analogue of `Gia_ManOriginsDupIf`: reset/alloc `pNew->vOrigins`, copy
   `nOriginsMax`, iterate the Nf correspondence, and
   `Gia_ObjUnionOrigins(pNew, iNewObj, p, i)` for each mapped node. Declare in
   `src/aig/gia/gia.h` next to the existing `Gia_ManOriginsDup*` prototypes.

3. **Sanity-check `&write` for mapped GIAs (`giaAiger.c`).**
   Confirm the `"y"` writer (line 1876) indexes objects consistently when the
   GIA also carries `vCellMapping` (mapped POs / cell boxes). Adjust the object
   walk only if mapping changes the object numbering assumptions.

4. **Cover the remaining std-cell `&`-space steps.** The LibreLane area script
   uses `&get -n; &st; &dch; &nf; &put`. `&st`/`&dch` are already instrumented;
   confirm `&put` (GIA→`Abc_Ntk_t`) is not on the critical path for the XAIGER
   route (it is only needed for the BLIF route, which we are not using for
   provenance). No `&put` change expected for this PR.

### Tests (ABC-level, no yosys needed)

Add `test/origins/stdcell_nf.sh` (or extend the existing origin tests):

- Build a small AIG with known per-object origins (reuse the PR #487 origin
  test fixtures), run `&get; &st; &dch; &nf -L <genlib>; &write -y out.xaig`,
  read back, and assert via `&origins` that mapped nodes carry the expected
  origin literals (non-empty, trace to the seeded sources).
- Negative control: same flow without instrumentation regressed → origins empty.
- Keep a tiny genlib (e.g. the bundled `mcnc.genlib`/a 4-gate library) so the
  test is hermetic and fast.

### Risks / open questions

- **`Nf_Man_t` correspondence.** Unlike `If_Man_t` (clean `pIfObj->iCopy`), the
  cut-based `&nf` mapper may not expose a 1:1 source→mapped-node map directly;
  may need to record it during `Nf_ManDeriveMapping`. This is the main unknown
  and should be spiked first.
- **Origin blow-up.** Std-cell mapping fans many AIG nodes into one cell; union
  semantics already dedup, and `nOriginsMax` (`&origins -M`) caps accumulation.
  Confirm the cap is honored on the `&nf` path.
- **Mapped-object indexing in `&write`** (item 3) — most likely fine, but verify.

## Companion yosys work (out of scope here, documented for the boundary)

To make a **LibreLane** run carry `\src` on std cells, yosys must consume origins
for standard-cell mapping. The blocker is the BLIF channel (loses object
identity). Options, lowest-risk first:

- **A. XAIGER std-cell round-trip.** Route std-cell mapping through a
  XAIGER write/read (as `abc9 -lut` does for LUTs) so object ids — and thus the
  `"y"` extension — survive, then apply `\src` to mapped std-cell instances
  (today `aigerparse.cc` applies only to `$lut`; extend to mapped boxes/cells).
  This is the real unlock and the bulk of the remaining effort. `abc9` is
  currently LUT-only, so this is non-trivial yosys work.
- **B. Origin sidecar keyed by stable net names.** Have the classic `abc` pass
  emit, alongside `output.blif`, an origin map keyed by the BLIF net/PO names
  (which survive the round-trip, as ABC's `dress` already relies on), and apply
  `\src` on the yosys side by net→driver correspondence. Avoids touching the
  mapping channel but adds a fragile naming dependency.

Recommendation: land this ABC PR (A's foundation, independently useful), then
pursue yosys option **A** to complete the LibreLane flow.

## Validation harness

Use the existing integration flake at `reference/origin-shell` (LibreLane
3.1.0.dev1 + this abc + `src-retention-y-ext` yosys):
- ABC-level: `nix build .#abc` then run the new `&nf` origin test.
- Once the yosys side lands: re-run `librelane --to Yosys.Synthesis` on a
  combinational design and assert `\src` on mapped std cells (mirror of
  `abc9_src_retention_full.sh`, `--cell-type` = sky130 cell types).
