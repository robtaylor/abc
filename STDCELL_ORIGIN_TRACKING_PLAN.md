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

## Maintainer guidance (authoritative)

From direct correspondence with Alan Mishchenko (ABC maintainer) and Dan
Ravenslofty (YosysHQ, final say on abc-in-yosys), and the PR threads:

- **The lightweight per-object `vOrigins` approach (PR #487) is endorsed.** Alan:
  "your implementation is quite clean and has low resource usage … Glad this
  method works for mapping RTL code into the resulting AIG nodes (as well gates
  and LUTs after mapping)." So origins on **mapped std-cell gates** are an
  accepted goal, not just LUTs.
- **Alan explicitly named the engines to cover**: `&dc2, &if, &nf, &mfs, &syn2,
  &dch, &synch2, &sweep, &scorr`. **`&nf` is on that list** — this PR is exactly
  the remaining item.
- **Acceptance criterion: zero change to default behavior.** Alan: "If the
  resulting integration does not change the default behavior, I will be happy to
  include it in the public version." The `&nf` change must be a no-op unless
  `p->vOrigins` is set.
- **The heavyweight `Nr_Man_t` retention manager is rejected — do NOT revive it.**
  Alan: "the idea of 'origin annotation' [hash-table manager] is hard to
  implement, because it requires modifications to 10+ different packages … it
  reminds me of [HAIG] which was one of the most complicated things I ever
  implemented in ABC — and it did not work." This rules out the
  `YosysHQ/abc#41` / `Silimate/abc#4` (`Nr_Man_t` on `Abc_Ntk_t`) lineage,
  including its classic-`abc`/`write_blif` provenance path. The `vOrigins`
  (GIA-only) line is the sanctioned successor.
- **Lofty originally suggested the XAIGER `"y"` extension** as the yosys↔abc
  channel (`YosysHQ/abc#41` discussion); the abc internals are "up to Alan".

### Current engine coverage (verified on `origin-tracking-clean`)

| Engine (Alan's list) | File | `vOrigins` covered |
|---|---|---|
| `&if` (LUT map) | `giaIf.c` | yes |
| `&mfs` | `giaMfs.c` | yes |
| `&sweep` | `giaSweep.c` | yes |
| `&scorr` | `cecCorr.c` | yes |
| `&dc2` / `&dch` | `giaAig.c` (`…AfterRoundTrip`) | yes |
| `&syn2` / `&synch2` | `giaScript.c` | yes |
| `&b` (balance) | `giaBalAig.c` | yes |
| **`&nf` (std-cell map)** | **`giaNf.c`** | **yes — by construction (see spike)** |

`grep -c Origin src/aig/gia/giaNf.c` = 0 — but this is **expected and fine**.

### Spike result (verified): `&nf` preserves origins without code changes

`&nf` maps **in place**: `Nf_ManDeriveMapping` attaches `vCellMapping` to
`p->pGia` and returns the *same* GIA (`giaNf.c:2409` → `return p->pGia`) — no
object renumbering. `Nf_StoCreate` only allocates side arrays; it does not dup
the GIA. The only renumbering operations in the `Nf_ManPerformMapping` wrapper
are `Gia_ManDupMuxes` (coarsen), `Gia_ManDupUnnormalize` and
`Gia_ManDupNormalize` (boxes path) — **all already origin-instrumented**. So
`vOrigins` on the AIG nodes survive `&nf` untouched.

**Empirically confirmed** (standalone abc, PR #487 binary, sky130 liberty, on a
yosys-produced XAIGER carrying the `"y"` extension):

```
&read input.xaig; &origins -M 100; &origins   -> Origins: 13 entries
&nf;                                &origins   -> Origins: 13 entries   (preserved)
```

Consequence: **no propagation code is needed in `giaNf.c`**, and a
`Gia_ManOriginsDupNf` helper is **not** required. Alan's "cover `&nf`" item is
satisfied by construction.

> Open/inconclusive: a plain standalone `&write`→`&read` round-trip of the
> *mapped* GIA did not restore origins, but that test was not faithful (missing
> `read_box`; the real abc9 write-back path differs). Whether `&write` emits the
> `"y"` extension correctly for a cell-mapped GIA must be checked on the
> yosys-integration side — it belongs to the consumption work below, not to
> `&nf` itself.

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

## ABC PR scope (this branch) — verification, not new code

The spike (above) shows `&nf` already preserves `vOrigins`. So there is **no
`giaNf.c` change and no `Gia_ManOriginsDupNf` helper**. The deliverable is a
**regression test** that locks in the behaviour, plus a one-line code comment.

### Changes

1. **Regression test** (`test/origins/stdcell_nf.sh` or extend the existing
   origin tests): seed origins from a yosys-produced XAIGER with the `"y"`
   extension (or an equivalent fixture), `read_lib <lib>; &read in.xaig;
   &origins -M 100; &nf; &origins`, and assert the post-`&nf` origin count is
   preserved (≈ pre-`&nf`, non-zero). Keep it hermetic with a small library.
2. **Comment in `giaNf.c`** noting that `&nf` maps in place and relies on the
   already-instrumented dups, so origins are preserved without explicit code —
   so a future reader doesn't "fix" a non-bug.

### Why this is right (and matches Alan's constraint)

Zero behaviour change by definition — there is no code path change at all. This
is the cleanest possible form of "cover `&nf`".

### Note on PR placement

Because this is verify-only (no engine code), the "split engines into separate
PRs" rationale does not really apply to `&nf` — there is nothing to review but a
test. Simplest: fold the test into #487 (origin-tracking-clean) and treat the
`&nf` engine-coverage item as closed. Engine-splitting matters for engines that
need real propagation code; `&nf` is not one.

## Companion yosys work (out of scope here, documented for the boundary)

To make a **LibreLane** run carry `\src` on std cells, yosys must consume origins
for standard-cell mapping. The blocker is the BLIF channel (loses object
identity). Options, lowest-risk first:

- **A. XAIGER std-cell round-trip.** Route std-cell mapping through a
  XAIGER write/read (as `abc9 -lut` does for LUTs) so object ids — and thus the
  `"y"` extension — survive, then apply `\src` to mapped std-cell instances
  (today `aigerparse.cc` applies only to `$lut`; extend to mapped boxes/cells).
  This is the real unlock and the bulk of the remaining effort.
- **B. Origin sidecar keyed by stable net names.** Have the classic `abc` pass
  emit, alongside `output.blif`, an origin map keyed by the BLIF net/PO names
  (which survive the round-trip, as ABC's `dress` already relies on), and apply
  `\src` on the yosys side by net→driver correspondence. Avoids touching the
  mapping channel but adds a fragile naming dependency.

### Upstream-home reality (important)

- **The yosys consumer side has no upstream home.** `YosysHQ/yosys#5712` (the
  `\src`-via-"y" consumer) was **closed** by YosysHQ staff on process +
  technical grounds. So the yosys/librelane integration lives in our fork
  (`robtaylor/yosys @ src-retention-y-ext`) + the `reference/origin-shell` flake.
- **"abc9 everywhere" is ruled out.** `YosysHQ/yosys#5679` (merged) *removed*
  `abc9 -liberty` because "our `abc9` command isn't set up to make use of Liberty
  files." abc9 is LUT-only upstream and nothing drives abc9+liberty; making
  abc9 the std-cell mapper would be a fully self-maintained yosys effort with
  real QoR-regression risk. Not pursued.
- **The abc side does have an upstream home** (`berkeley-abc/abc#487` + this
  follow-on): Alan is willing to take it provided default behavior is unchanged.

Recommendation: land this ABC PR (the sanctioned `&nf` item — independently
useful and upstreamable), then implement consumption as **fork-only** yosys work
via option **A** (preferred) or **B**. Do not block on, or attempt, abc9-everywhere.

## Validation harness

Use the existing integration flake at `reference/origin-shell` (LibreLane
3.1.0.dev1 + this abc + `src-retention-y-ext` yosys):
- ABC-level: `nix build .#abc` then run the new `&nf` origin test.
- Once the yosys side lands: re-run `librelane --to Yosys.Synthesis` on a
  combinational design and assert `\src` on mapped std cells (mirror of
  `abc9_src_retention_full.sh`, `--cell-type` = sky130 cell types).
