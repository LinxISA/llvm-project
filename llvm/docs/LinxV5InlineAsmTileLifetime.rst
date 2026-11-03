Automatic Tile lifetimes in inline assembly
==========================================

The ``linxv5-inline-asm-tile-lifetime`` machine pass recognizes a restricted
straight-line subset of existing Tile inline assembly. TileOP headers and
public APIs do not need new modifiers or source changes. The pass runs after
pseudo expansion and before Local Tile relative-reference conversion.

The pass computes physical Tile liveness to a CFG fixed point, including
loop backedges and successor uses. For recognized ``B.IOT`` and ``B.IOS``
source placeholders, it retains a source when it is live after the asm or
referenced again within the asm. Repeated reads of one source in the same
bundle conservatively retain all occurrences; sequential distinct bundles
can consume the final occurrence. Destination placeholders are unchanged.

Rewriting uses internal ``P`` (retain) and ``Q`` (consume) modifiers rather
than appending text to register names. These remove any register-name reuse
suffix before printing the selected lifetime, avoiding duplicate suffixes.
Explicit lifetime modifiers and textual ``.reuse`` remain unchanged. The
pass does not interpret arbitrary assembly or promise to fix explicitly
incorrect lifetimes.

The recognized instruction vocabulary is ``BSTART.TLSU``, ``BSTART.TEPL``,
``B.DIM``, ``B.DATR``, ``B.IOR``, ``B.IOT`` and ``B.IOS``. Unknown instructions,
directives, labels, comments, semicolon-separated statements, dialect
alternatives and escaped dollar syntax leave the entire template unchanged.
Constant ``.if`` / ``.elseif`` equality conditions, ``.else`` and ``.endif``
are evaluated before recognition. Operands must be decimal integers or
immediate ``${N:c}`` references; nesting is limited to sixteen levels.
Leading-zero integer spellings are left untouched to avoid confusing
assembler octal literals with decimal constants.
Only the selected arm participates in lifetime inference. Unresolved,
unsupported or malformed conditions leave the entire template unchanged.
Only known read-only TLSU forms (TMOV, TMOV.S2L.BROADCAST,
TMOV.S2L.EXTRACT and TSTORE) can consume sources. Other TLSU operations,
including associated TLOAD, retain their recognized sources.

Partial or nonliteral PE masks, a partial function PE domain, generic-element
mode, ``B.IOR`` (whose execution-mask role is not inferred), tied or overlapping
definitions, literal source registers, and Tile registers involved in COPY
or memory operations conservatively prevent inferred consumption. Functions
containing machine calls or an unrecognized inline assembly template also
retain inferred sources. A literal source reference in another template
likewise prevents inferred consumption, since it has no machine operand
participating in liveness analysis. Allocator-required retain decisions take
precedence over an inferred consume modifier. These restrictions
avoid treating an escaped Shared identity or a partially executed consumer
as a proven final use. They are coverage limits rather than optimal lifetime
analysis for every API or aliasing pattern.

Integration status
------------------

TileOP PE-mask macros expand to assembler ``.if`` / ``.elseif`` directives.
Constant full-PE instances are recognized through condition normalization;
partial-PE instances retain their sources. An unrecognized template still
prevents inferred consumption elsewhere in its function. This intentionally
restricted coverage must not be described as arbitrary inline-asm support.

The real API integration probe loads a Local Tile, publishes a Shared Tile,
then calls ``TMOV_S2L_BROADCAST`` twice with the same Shared value. The desired
full-PE final-use pattern is retain followed by consume. With unchanged
TileOP headers, this probe generates that pattern and compiles to an object.
Disassembly tests also check lifetime encoding for constant-condition
templates. These compiler checks are not a successful model execution gate.
A compatible model lifetime/publication-order test and broader alias
validation remain necessary before claiming general end-to-end readiness.

``-linxv5-enable-inline-asm-tile-lifetime=false`` disables rewriting. For Clang, pass
the option through ``-mllvm``. Unsupported templates keep the existing
behavior; this fallback is not a new correctness guarantee for arbitrary
inline assembly. Model execution and publication-order validation remain
separate from compiler and object-encoding tests.
