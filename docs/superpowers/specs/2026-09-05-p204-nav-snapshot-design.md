# P2-04 approved snapshot contract

The loader returns only an owning shared_ptr<const NavMeshSnapshot> after complete
decode, semantic validation and strict EOF validation, in that priority order.
Semantic diagnostics select the earliest exact file field offset. Area/hiding
IDs are nonzero and globally unique (v1 hiding has no ID). All retained references
must resolve and be nonzero. Only connections reject self references and repeated
targets within one direction; cross-direction repetition and tactical owner
references (including previous == next) are valid.

All geometry is finite; NW.x < SE.x and NW.y < SE.y are required, without Z order
constraints. Encounter directions are 0..3. Attributes, hiding flags, approach
traversal and encounter t retain their raw bytes. V5 Place is 0 (absent) or a
1-based dictionary entry. V1/2 discarded encounters remain decode-only.

Explicit limits cover input, header, areas and logical snapshot bytes. Zero is
not unlimited. Logical bytes are sizeof(snapshot), all retained record/string
objects and string payload including NUL, with checked arithmetic before each
count-driven allocation. This excludes allocator/capacity/shared-control overhead
and temporary validation data; temporary data is bounded by decoded counts.

An internal read context records field offsets and raw scalar values, charges
the logical budget and converts its own allocation failures to diagnostics.
Existing standalone reader signatures remain available. Context is query-local,
never global and never published. Builder is internal; snapshot offers const
header/area access only and retains wire order, not input-buffer references.

No spatial index, route queries, main merge, remote operation or Linux/live test.

MSVC Nav and its consumers use PUBLIC _ITERATOR_DEBUG_LEVEL=0, including Debug.
MSVC's debug iterator proxy allocates inside noexcept vector construction/move;
failure there terminates before any catch can run. Disabling this instrumentation
is necessary for the allocation contract; Debug assertions and /W4 /WX remain.
Consumers must keep the same STL ABI via the exported CMake definition.

Error mapping: zero IDs, self/repeated connections and invalid Place use
InvalidValue; duplicate definitions use DuplicateId; missing nonzero targets use
DanglingReference; direction values above 3 use UnsupportedValue. Geometry uses
the first offending SE coordinate. Nonfinite values remain decoder errors.
Allocation in semantic lookup construction uses the first area ID as context;
snapshot/control-block allocation uses RawInput/RawBytes at offset zero.
