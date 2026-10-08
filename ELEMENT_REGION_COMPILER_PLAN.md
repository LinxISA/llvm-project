# PTO element-for: standard LLVM region compiler

This implementation plan follows the user-requested Astra/xhigh source audit.
It is a toolchain plan, not another normative ISA specification. Instruction
legality remains owned by the linked PTO ASL/NDF. Implementation is still open.

## Outcome and baseline

`#pragma pto element for` must enter ordinary C++ CodeGen and a required LLVM
region pass. Source spelling of `if`, local names, or a particular histogram
kernel must not determine whether lowering works. Public TileOp API owns logical
views; existing Tile instruction selection and register allocation own physical
execution and intermediates. No private benchmark header or copied operator is
needed.

Current baseline: V5 LLVM15.0.4 worktree head74c93843, with local reviewed checkpoint23269942 for ordinary gather/backend metadata.
The checkpoint is preserved for comparison, not the final standard region compiler. The current
`EmitLinxElementwiseForStmt` is an AST matcher, despite its expression DAG; it
is not the requested generic CFG compiler.

Existing SIMT implementation: `/Users/zhoubot/linx-isa/compiler/llvm`, clean
head8121b26b9beeabfc0bb6967aab2c67aa52b40638, LLVM23.0.0. Its target is LinxISA,
not the active LinxV5. Reuse is a bounded adaptation of proven analysis, not
wholesale copying of the 7k-line pass or enabling a V5 flag.

## Reuse boundary

From `LinxISASIMTAutoVectorize.cpp`, reuse loop scaffold/SCEV induction analysis,
CFG diamond and PHI/store-merge planning, speculation checks, recurrence/liveout
classification and their IR test shapes. Add AA/MemorySSA dependence proofs;
the existing pass only requests LoopInfo/ScalarEvolution and does not provide
those general memory proofs.

Do not transfer B.TEXT/vblock_launch/body assembly, manual physical register
names, scalar recurrence replay, ignored dummy/exit calls, or width truncation.
V5 ElementwiseMask/StructurizeCFG/AnnotateControlFlow may supply algorithms, but
its hardware SIMT if/flow/loop intrinsics are not PTO Tile-region execution.

## Compiler pipeline

1. Preserve parser/Sema validation. Emit normal ForStmt CFG, loop/source metadata
   and a required region sentinel. Prevent generic unroll/vectorization from
   erasing the marked region before its compiler pass.
2. Preserve the public view contract in IR through reference return, aliases,
   helper inlining and optimization. Track actual pointer/storage identity,
   semantic dtype, logical shape, backing geometry and supplied range data.
   Never infer layout from vector width or invent noalias. Accessor argument
   evaluation and side effects must remain exactly once.
3. Use LoopInfo, dominators/postdominators and ScalarEvolution to prove the
   iteration domain. Normalize descriptor-backed vector extract/insert accesses
   before classifying whole-vector PHIs as recurrences. Locals use normal SSA.
4. Analyze edge/block predicates and merge PHIs generically. Preserve short
   circuit structure, source casts, overflow and within-element effect order.
   Use isSafeToSpeculativelyExecute; inactive loads/atomics and guarded division
   need real masks, not speculation followed by select.
5. Use AA/MemorySSA and affine dependence proofs. Unknown conflicting scatter,
   cross-element recurrence, unsupported calls/types/shapes and ordered atomic
   dependencies diagnose before committing the complete region transform.
6. Emit the existing typed ew/Tile intrinsics, TLEA and ordinary/atomic memory
   operations. Keep temporary allocation in the existing Tile backend. Replace
   function-wide elementwise load/store layout inference with access/value
   descriptors.
7. Register required normal-Clang and llc lowering before AtomicExpand, including
   O0/optnone handling. Verify before ISel that no region/view sentinel, marked
   scalar loop or forbidden scalarized logical-view access survived. No fallback
   to the AST matcher when canonical region lowering rejects.

For static v1 view metadata, reuse LLVM15 llvm.ptr.annotation around the already
evaluated reference-return pointer in CGExpr::EmitCallExprLValue. Never evaluate
an accessor argument twice. The required pass must record descriptors, replace
annotations with operand0, remove them, then rebuild analyses and run scoped
promotion: SCEV sees through pointer annotations, but AA/getUnderlyingObject do
not, and an unconsumed annotation blocks SROA/mem2reg. O0 only inlines accessors
marked AlwaysInline; scoped attributes on the existing public getters/accessor
and required promotion are needed. Do not globally remove optnone.

Existing ew.tcmp hardcodes NORM, and ew.tsel has no demonstrated U32/M32 closure.
Therefore generic branch PHIs/select and guarded arithmetic are explicit P2
backend/model gates, not capabilities that the first straight-line P1a slice
may claim. Memory predicates continue to require actual access suppression.

## Bounded implementation and acceptance

P1a: complete element_expression_chain through normal IR, preserving all ten
U32 binary operations, unary operations, long-lived/reused intermediates and
three TileOp/element regions. Demonstrate equivalent induction forms, local
reassignment, harmless reference aliases and inlined arithmetic helpers.

P1b: complete indexed_gather_tile_element through CFG-derived predicates,
TLEA and ordinary GPR-masked MGATHER. Keep the 263-element result, count257
probe with three entirely inactive parts, UINT32_MAX poison indices, both
output guards, zero diagnostics and post-gather Tile +3. Current emitter only
supports zero-inactive ordinary gather; merge/select operations require their
own validated backend contract rather than guessing a missing merge operand.

P2: actual histogram, selected-radix and full Top-K regions through the same
plan; generic comparisons/mask composition/PHIs and relaxed atomic add. Preserve
atomic return-result association and independent per-bin permutation oracles.
Remove canonical AST body recognizers only after parity and the repeated17-call
Top-K case pass. No benchmark-specific call whitelist.

P3: real typed views, F32/S32/narrow/b64 geometry, rank/stride memory, scatter and
per-element probing loops in independently validated slices. Do not simply
remove U32 assertions or reinterpret raw carrier references. Public partitions
are striped (valid_size=ceil((valid-part)/4), output base+part), not contiguous
chunks. Input validity does not automatically suppress all expressions: padded
expression output remains part of the source algorithm.

P4: migrate all21 actual applications and original configurations from the
bench inventory. Float gather and int64 hash table cannot become U32 examples
just to fit the first compiler slice. All21 entries remain pending until each
complete kernel/configuration passes its own evidence.

## Verification

Raw frontend IR: normal CFG plus durable view/region contracts.
Pass IR tests: SSA/PHI, aliases, control/memory dependence, diagnostic boundaries.
Object/MIR: native Tile registers/opcodes, exactly correct TLEA scaling and
ordinary gather, no SIMT body launch or scalar fallback. O0/O2/O3, inlining and
bitcode roundtrip must preserve the mandatory contract.

Fresh same ELF on gfrun and gfsim against independent goldens, with final
queue conservation OK, A3=0 and no error/assert. Do not enlarge resources,
watchdogs, weaken validators or change an oracle to conceal a failure.

Accounting: earlier suite7 plus separate main Top-K =8 historical passing
ELFs. New gather makes suite8 and total9 once execution is proved; it does not
close the original gather application entry. At this plan snapshot the new
case passes actual installed API compile/IR/disassembly/gfrun, and the same ELF now passes all four independent gfsim segments after
shape repair (queue1592/1592,A3=0). Fresh clean-head full harness is pending.
Default model regression before lifetime repair
is899/900; no full-completion claim is made.

## Typed B32 continuation after checkpoint 697f17

The next delivery extends the same region compiler to a true S32 profile before
adding F32. View dtype remains explicit throughout preparation, expression
validation, Tile SSA and ND2M32/M322ND transport. U32 gather/atomic eligibility
must explicitly require U32 views; accepting another arithmetic dtype must not
silently enable its memory or predicate profile.

S32 scalar splats reuse typed TCI. Add/subtract/multiply and bitwise operations
retain their LLVM bit semantics; signed division uses TDIV and arithmetic right
shift uses TSHR under S32. Source C++ `srem` lowers to TDIV, TMUL, TSUB: PTO TREM
has divisor-sign floor modulo, so it cannot implement source `%` directly.
Mixed view profiles and unsupported casts/CFG remain diagnosed. Native backend
validation must match dtype, v32i32, 32x1 geometry and M32 layout explicitly.

Acceptance includes source/IR/object tests, negative dtype/profile combinations,
a complete unchanged-size S32 TileOp/element/TileOp benchmark with independent
signed division/remainder/shift goldens, and same-ELF gfrun/gfsim checks. Model
numeric corrections are separately reviewed against owning ASL at cab1978;
models' raw low32 result helpers do not redefine source C++ signed overflow.
F32 requires a separate exact TEXPANDS and native TNEG closure rather than
reinterpret casts or `0.0 - x`. No application/configuration entry closes merely
because a typed foundation example passes.

## P0 history reconciliation (2026-10-08)

The local `codex/pto-element-region-compiler` branch merged PR117 head
`7be28644a0c937d05723a12127b73875e415311f` with a normal merge commit. The
common ancestor was `34ade53ebfe81975a91512568e337bacb91ad27d`; the local
pre-merge head was `9b43f05430eb757c1e14d6fe659fcfefee7a0206`.

| Source | Kept | Reconciliation |
| --- | --- | --- |
| Local `9b43f054` | ordinary `EmitForStmt` region entry, typed view descriptors, S32 Tile SSA and diagnostics | authoritative for frontend entry and typed expression conflicts |
| PR117 `7be28644` | `7a120731` multi-definition Tile CFG join fix, exceptional-control-flow rejection, block-name-independent tests and gather test hardening | merged without restoring `EmitLinxElementwiseForStmt` or its AST/body recognizers |
| `dev-llvm15_56` `5c16f442` | comparison only | not merged; its only post-PR117 source change is the separate Local TMOV binder change in `LinxV5EmitHeader.cpp` |

The reconciled tree must continue to satisfy both negative conditions:
`CGStmt.cpp` contains no `EmitLinxElementwiseForStmt`, and marked loops always
enter ordinary Clang CFG emission before the required LLVM region pass.
