"""Versioned, expanded LLVM TileOp model and deterministic C++ table writer.

The local seed preserves the previously checked-in catalog. Emitter-specific
forms use an explicit Encoding attribute so assembly never guesses a binder
layout from otherwise identical logical operands.
"""

from copy import deepcopy
import hashlib
import json
from pathlib import Path

SEED = Path(__file__).with_name("tile_macro_catalog.json")


def config(field, kind, slot, default=None, dimension=False, optional=True):
    return [
        field,
        field + "?",
        default,
        None,
        None,
        None,
        dimension,
        optional,
        [[kind, slot, -1]],
    ]


def load_model():
    return json.loads(SEED.read_text())


def extend_model(model):
    for op in model["operations"]:
        for f in op["forms"]:
            f.setdefault("encoding", None)
            fields = {c[0] for c in f["configs"]}
            if op["family"] == "CUBE" and "CCTRL" not in fields:
                f["configs"].append(config("CCTRL", "DATR", "PadValueOrByteId", "None"))
            if op["name"] in {"TMOV", "TPACK", "TSHUF", "TPERMUTE", "TUNPACK"}:
                for field, slot in [
                    ("Col", "LB2"),
                    ("ValidRow", "LB1"),
                    ("ValidCol", "LB0"),
                ]:
                    f["configs"].append(config(field, "DIM", slot, "1", True))
            if (
                op["name"]
                in {
                    "TMOV",
                    "TPACK",
                    "TSHUF",
                    "TPERMUTE",
                    "TUNPACK",
                    "TCI",
                    "TLOAD",
                    "MGATHER_ADD",
                    "MSCATTER_MASK",
                }
                and "PadValue" not in fields
            ):
                f["configs"].append(
                    config("PadValue", "DATR", "PadValueOrByteId", "Null")
                )
            if op["name"] == "TCI" and "Layout" not in fields:
                f["configs"].append(config("Layout", "DATR", "Layout"))
    add_emitted_forms(model)
    add_address_forms(model)
    add_execution_mask_forms(model)
    add_tile_execution_mask_forms(model)
    add_indexed_tlsu_range_forms(model)
    align_normative_catalog(model)
    for op in model["operations"]:
        if op["name"] in {"TGEMV", "TGEMV_MX"}:
            for base in list(op["forms"]):
                f = deepcopy(base)
                next(c for c in f["configs"] if c[0] == "M")[3] = "NonUnitM"
                f["id"] += "-nonunit-m"
                f["encoding"] = "E" + str(len(op["forms"]))
                op["forms"].append(f)


def align_normative_catalog(model):
    normative = json.loads(SEED.with_name("pto_0590_catalog.json").read_text())
    operations = {operation["name"]: operation for operation in model["operations"]}
    for operation in normative["operations"]:
        actual = operations[operation["name"]]
        if (actual["family"], actual["selector"]) != (operation["family"], operation["selector"]):
            raise ValueError("normative header mismatch: " + operation["name"])
        for form in operation["forms"]:
            if not form["spelling"].startswith("TSTORE.ATOMIC_"):
                continue
            imported = deepcopy(form)
            imported["canonical"] = True
            imported["encoding"] = "E" + str(len(actual["forms"]))
            for entry in imported["configs"]:
                if entry[0] == "CMode":
                    entry[3] = "Atomic" + entry[2]
                if entry[0] == "Canonicalize":
                    entry[3] = "1"
            actual["forms"].append(imported)
    model["architecture"] = normative["architecture"]
    model["normative_source_sha256"] = normative["source_sha256"]


def add_indexed_tlsu_range_forms(model):
    """PTO #424 / 1981bceffa82: optional S64 relative byte bounds.

    The seed stays at 0.58.7; this is a scoped extension, not a catalog upgrade.
    Preserve every old encoding ID and require both bound operands explicitly.
    """
    for op in model["operations"]:
        if not op["name"].startswith(("MGATHER", "MSCATTER")):
            continue
        ordinary = [f for f in op["forms"]
                    if not any(c[0] == "ExecMask" for c in f["commands"])]
        for base in ordinary:
            for mask_words in (0, 1, 2):
                f = deepcopy(base)
                f["id"] += "-range" + (f"-mask{mask_words}" if mask_words else "")
                f["encoding"] = "E" + str(len(op["forms"]))
                address = next(i for i, b in enumerate(f["bindings"])
                               if b[2] == "GPRListSource")
                binding = f["bindings"][address]
                f["format"] = f["format"].replace(
                    binding[1], "[BaseGPR, MinIndexGPR, MaxIndexGPR]")
                binding[1] = "[BaseGPR, MinIndexGPR, MaxIndexGPR]"
                binding[3] += [[slot, None, None, False, 0]
                               for slot in ("RegSrc1", "RegSrc2")]
                command = next(c for c in f["commands"] if c[0] == "IOR")
                command[2] += [[address, 1], [address, 2]]
                if mask_words and not any(c[0] == "PadValue" for c in f["configs"]):
                    f["configs"].append(config("PadValue", "DATR", "PadValueOrByteId", "Null"))
                if mask_words:
                    f["configs"] += [config("PredInv", "DATR", "PredInv", "0"),
                                     config("ZeroMask", "DATR", "ZeroMask", "0")]
                    f["format"] += ", ExecMask[" + ", ".join(
                        ["MaskGPR"] * mask_words) + "]"
                    mask_index = len(f["bindings"])
                    f["bindings"].append([
                        "ExecMask", "ExecMask", "GPRListSource",
                        [["RegSrc" + str(i), None, None, False, 0]
                         for i in range(mask_words)]])
                    position = f["commands"].index(command) + 1
                    f["commands"].insert(position, ["ExecMask", position,
                        [[mask_index, i] for i in range(mask_words)]])
                op["forms"].append(f)
        # Append generic PredicateTile carriers after all existing encodings.
        # They use a final IOT, leaving the range as the only IOR.
        for base in list(op["forms"]):
            if not base["id"].endswith("-range"):
                continue
            attrs = [config("PredInv", "DATR", "PredInv", "0"),
                     config("ZeroMask", "DATR", "ZeroMask", "0")]
            if any(b[2] == "PredicateTileSource" for b in base["bindings"]):
                base["configs"] += attrs
                continue
            f = deepcopy(base)
            f["id"] += "-predicate"
            f["encoding"] = "E" + str(len(op["forms"]))
            f["configs"] += attrs
            if not any(c[0] == "PadValue" for c in f["configs"]):
                f["configs"].append(config("PadValue", "DATR", "PadValueOrByteId", "Null"))
            index = len(f["bindings"])
            f["bindings"].append(["ExecutionPredicate", "ExecutionPredicate",
                                  "PredicateTileSource",
                                  [["Role0", None, None, False, 1]]])
            f["format"] += ", ExecutionPredicate"
            iots = [i for i, c in enumerate(f["commands"]) if c[0] == "IOT"]
            ordinal = max(f["commands"][i][1] for i in iots) + 1
            f["commands"].insert(iots[-1] + 1, ["IOT", ordinal, [[index, 0]]])
            next(c for c in f["configs"] if c[0] == "PEMask")[8].append(
                ["IOT", "PEMask", ordinal])
            op["forms"].append(f)
        if op["name"] == "MGATHER_MASK":
            # The intrinsic emitter binds index+destination, then predicate.
            # Keep this distinct from split and fully combined seed forms.
            base = next(f for f in op["forms"] if f["id"].endswith("-range"))
            f = deepcopy(base)
            f["id"] += "-result-first"
            f["encoding"] = "E" + str(len(op["forms"]))
            f["commands"] = [["IOR", 0, [[0, 0], [0, 1], [0, 2]]],
                             ["IOT", 0, [[1, 0], [3, 0]]],
                             ["IOT", 1, [[2, 0]]]]
            f["bindings"][3][3][0][0] = "Role1"
            op["forms"].append(f)



def variant(op, base, suffix, groups):
    """Retain logical operand order; replace only the physical binder plan."""
    f = deepcopy(base)
    f["id"] += "-" + suffix
    f["encoding"] = "E" + str(len(op["forms"]))
    fields = {b[0]: i for i, b in enumerate(f["bindings"])}
    used = set()
    commands = []
    for group in groups:
        kind, names = group
        pairs = []
        for slot, name in enumerate(names):
            i = fields[name]
            used.add(i)
            f["bindings"][i][3][0][0] = "Role" + str(slot)
            pairs.append([i, 0])
        commands.append([kind, len(commands), pairs])
    for c in f["commands"]:
        rest = [p for p in c[2] if p[0] not in used]
        if rest:
            commands.append([c[0], len(commands), rest])
    f["commands"] = commands
    op["forms"].append(f)
    return f


def add_emitted_forms(model):
    for op in model["operations"]:
        name = op["name"]
        base = op["forms"][0]
        if name in {"GMOV", "TMOV"} or name.startswith(("MGATHER", "MSCATTER")):
            # Merge adjacent IOT source/destination groups, with at most two
            # source selectors per physical record. Keep IOR/IOS unchanged.
            f = deepcopy(base)
            pending = []
            commands = []
            for c in f["commands"]:
                if c[0] == "IOT":
                    pending.extend(c[2])
                else:
                    commands.append(c)
            chunks = []
            sources = [
                p
                for p in pending
                if f["bindings"][p[0]][2]
                not in {
                    "LocalDestination",
                    "PredicateTileDestination",
                    "PredicateCellDestination",
                }
            ]
            dest = [p for p in pending if p not in sources]
            while len(sources) > 2:
                chunks.append(sources[:2])
                sources = sources[2:]
            chunks.append(sources + dest)
            f["commands"] = commands + [["IOT", i, p] for i, p in enumerate(chunks)]
            for c in f["commands"]:
                for slot, p in enumerate(c[2]):
                    if c[0] == "IOT":
                        f["bindings"][p[0]][3][p[1]][0] = "Role" + str(slot)
            f["id"] += "-combined"
            f["encoding"] = "E" + str(len(op["forms"]))
            if name == "MSCATTER_MASK":
                # The current emitter transports the dense predicate through
                # any local tile bank, rather than the legacy U-only carrier.
                for binding in f["bindings"]:
                    if binding[2] == "PredicateTileSource":
                        binding[2] = "LocalSource"
            if f["commands"] != base["commands"]:
                op["forms"].append(f)
        if name == "TSEL":
            variant(
                op,
                op["forms"][2],
                "combined",
                [("IOT", ["SrcTile1", "SrcTile2", "DstTile"])],
            )
        if name == "TCVT":
            for original in list(op["forms"][1:]):
                f = deepcopy(original)
                f["id"] = f["id"].replace("-split", "-combined")
                f["encoding"] = "E" + str(len(op["forms"]))
                f["commands"] = [["IOT", 0, [[0, 0], [1, 0]]]]
                f["bindings"][1][3][0][0] = "Role1"
                op["forms"].append(f)
        if name == "TGEMV_ACC":
            variant(
                op,
                base,
                "emitted-local-tail",
                [("IOT", ["AccTile"]), ("IOT", ["SrcVector", "SrcMatrix", "DstTile"])],
            )
        if name == "TMATMUL_MX":
            # Exact Shared-B + scale-B, Local-A + scale-A stream.
            f = deepcopy(op["forms"][-1])
            # Build the local/shared representation from the existing
            # shared-both-scales fields, retaining all numeric attributes.
            for binding in f["bindings"]:
                if binding[0] in {"SharedA", "ScaleA"}:
                    binding[2] = "LocalSource"
            f["commands"] = [
                ["IOS", 0, [[2, 0]]],
                ["IOS", 1, [[3, 0]]],
                ["IOT", 2, [[0, 0], [1, 0], [4, 0]]],
            ]
            for slot, i in enumerate([0, 1, 4]):
                f["bindings"][i][3][0][0] = "Role" + str(slot)
            f["id"] += "-local-a"
            f["encoding"] = "E" + str(len(op["forms"]))
            op["forms"].append(f)
            # The packed shared groups and the expanded four-IOS form have
            # identical physical roles. Use the grouped spelling, which can
            # now express each group member independently.
            next(
                f for f in op["forms"] if f["id"].endswith("emitted-shared-both-scales")
            )["canonical"] = False
        if name == "TMATMUL_MX_ACC":
            # Plain five-source emitter orders the mathematical operands
            # before the accumulator, unlike its fixpipe overload.
            b = op["forms"][0]
            variant(
                op,
                b,
                "emitted-local-acc-tail",
                [
                    ("IOT", ["SrcTile0", "RowScaleTile"]),
                    ("IOT", ["SrcTile1", "ColumnScaleTile"]),
                    ("IOT", ["AccTile", "DstTile"]),
                ],
            )
            # Shared-A/local-B and local-A/shared-B have indistinguishable
            # physical streams. Describe their transport roles explicitly;
            # runtime matrix provenance is not recoverable from these bytes.
            for shared_a in [False]:
                f = deepcopy(b)
                for binding in f["bindings"]:
                    if binding[0] in (
                        {"SrcTile0", "RowScaleTile"}
                        if shared_a
                        else {"SrcTile1", "ColumnScaleTile"}
                    ):
                        binding[2] = "SharedSource"
                a = ["SrcTile0", "RowScaleTile"]
                bb = ["SrcTile1", "ColumnScaleTile"]
                groups = [("IOS", [x]) for x in (a if shared_a else bb)]
                groups += [
                    ("IOT", bb if shared_a else a),
                    ("IOT", ["AccTile", "DstTile"]),
                ]
                emitted = variant(op, f, "emitted-shared-acc-tail", groups)
                transport_names = {
                    "SrcTile0": "LocalMatrix",
                    "RowScaleTile": "LocalScale",
                    "SrcTile1": "SharedMatrix",
                    "ColumnScaleTile": "SharedScale",
                }
                for binding in emitted["bindings"]:
                    binding[0] = transport_names.get(binding[0], binding[0])
        if name == "TMATMUL":
            variant(
                op,
                op["forms"][1],
                "split-shared-output",
                [("IOT", ["LocalLeftGroup"]), ("IOT", ["DstTile"])],
            )
            f = deepcopy(base)
            dst = next(i for i, b in enumerate(f["bindings"]) if b[0] == "DstTile")
            f["commands"][0][2] = [p for p in f["commands"][0][2] if p[0] != dst]
            index = next(
                i
                for i, c in enumerate(f["commands"])
                if any(f["bindings"][p[0]][0] == "RowMaxOut" for p in c[2])
            )
            f["commands"].insert(index, ["IOT", index, [[dst, 0]]])
            f["id"] += "-output-tail"
            f["encoding"] = "E" + str(len(op["forms"]))
            op["forms"].append(f)
            # Vector quantization and activation parameters share one IOT.
            f = deepcopy(f)
            quant = next(
                c
                for c in f["commands"]
                if any(f["bindings"][p[0]][0] == "QuantParamTile" for p in c[2])
            )
            relu = next(
                c
                for c in f["commands"]
                if any(f["bindings"][p[0]][0] == "ReluParamTile" for p in c[2])
            )
            quant[2] += relu[2]
            f["commands"].remove(relu)
            f["id"] += "-packed-params"
            f["encoding"] = "E" + str(len(op["forms"]))
            # Both operands must be active to distinguish this from output-tail.
            next(c for c in f["configs"] if c[0] == "PostMode")[3] = "3"
            next(c for c in f["configs"] if c[0] == "PreMode")[3] = "VectorParameter"
            op["forms"].append(f)
        if name == "TMATMUL_ACC":
            variant(
                op,
                base,
                "emitted-local-tail",
                [("IOT", ["AccTile"]), ("IOT", ["SrcTile0", "SrcTile1", "DstTile"])],
            )
            for idx, shared in [(1, False), (2, True)]:
                b = op["forms"][idx]
                groups = [("IOS", ["SharedLeftGroup"])] if shared else []
                groups += [("IOS", ["SharedRightGroup"])]
                groups += [
                    (
                        "IOT",
                        ["AccTile"]
                        + ([] if shared else ["LocalLeftGroup"])
                        + ["DstTile"],
                    )
                ]
                variant(op, b, "emitted-tail", groups)
        if name in {"TMATMUL_ACC", "TMATMUL_MX_ACC"}:
            for b in list(op["forms"]):
                if not any(x[0] == "CScaleTile" for x in b["bindings"]):
                    continue
                # Attach conditional CScale to the destination record where
                # the emitter uses an otherwise unused source selector.
                f = deepcopy(b)
                cs = next(
                    i for i, x in enumerate(f["bindings"]) if x[0] == "CScaleTile"
                )
                dst = next(
                    (
                        c
                        for c in f["commands"]
                        if c[0] == "IOT"
                        and any(
                            f["bindings"][p[0]][2] == "LocalDestination" for p in c[2]
                        )
                    ),
                    None,
                )
                if dst is None or len(dst[2]) >= 3:
                    continue
                for c in f["commands"]:
                    c[2] = [p for p in c[2] if p[0] != cs]
                dst[2].insert(len(dst[2]) - 1, [cs, 0])
                f["commands"] = [c for c in f["commands"] if c[2]]
                for c in f["commands"]:
                    if c[0] == "IOT":
                        for slot, p in enumerate(c[2]):
                            f["bindings"][p[0]][3][p[1]][0] = "Role" + str(slot)
                # Disjoint from its base: CScale must actually be enabled.
                next(c for c in f["configs"] if c[0] == "CScale")[3] = "1"
                f["id"] += "-cscale-tail"
                f["encoding"] = "E" + str(len(op["forms"]))
                op["forms"].append(f)


def add_address_forms(model):
    def member(slot):
        return [slot, None, None, False, 0]

    def form(name, configs, bindings, commands):
        return dict(
            spelling=name,
            format=name,
            id="pto0587-" + name.lower(),
            shape="None",
            canonical=True,
            configs=configs,
            bindings=bindings,
            commands=commands,
            encoding=None,
        )

    common = [
        config("DataType", "Header", "DataType", optional=False),
        config("Layout", "DATR", "Layout"),
        config("PadValue", "DATR", "PadValueOrByteId", "Null"),
        config("PEMask", "IOT", "PEMask", "AllPE"),
    ]
    dims = [
        config("Col", "DIM", "LB2", "1", True),
        config("ValidRow", "DIM", "LB1", "1", True),
        config("ValidCol", "DIM", "LB0", "1", True),
    ]
    lea = form(
        "TLEA",
        deepcopy(common) + deepcopy(dims[1:]),
        [
            ["IndexTile", "IndexTile", "LocalSource", [member("Role0")]],
            ["ElementBits", "ElementBits", "GPRSource", [member("RegSrc0")]],
            ["DstTile", "->DstTile<Size>", "LocalDestination", [member("Role1")]],
        ],
        [["IOT", 0, [[0, 0], [2, 0]]], ["IOR", 1, [[1, 0]]]],
    )
    model["operations"].append(
        dict(name="TLEA", family="TEPL", selector=46, forms=[lea])
    )
    img = form(
        "TIMG2COL",
        deepcopy(common) + deepcopy(dims),
        [
            ["Descriptor", "Descriptor", "GPRSource", [member("RegSrc0")]],
            [
                "ImageParams",
                "[base, outputPos, channelPos]",
                "GPRListSource",
                [member("RegSrc0"), member("RegSrc1"), member("RegSrc2")],
            ],
            ["DstTile", "->DstTile<Size>", "LocalDestination", [member("Role0")]],
        ],
        [
            ["IOR", 0, [[0, 0]]],
            ["IOR", 1, [[1, 0], [1, 1], [1, 2]]],
            ["IOT", 2, [[2, 0]]],
        ],
    )
    model["operations"].append(
        dict(name="TIMG2COL", family="TLSU", selector=28, forms=[img])
    )
    shared = deepcopy(img)
    shared["id"] += "-shared"
    shared["bindings"][2][0] = "DstShared"
    shared["bindings"][2][2] = "SharedDestination"
    shared["commands"][-1][0] = "IOS"
    for c in shared["configs"]:
        if c[0] == "PEMask":
            c[8][0][0] = "IOS"
    model["operations"][-1]["forms"].append(shared)


def add_execution_mask_forms(model):
    # Execution records use dense source slots: existing scalar operands first,
    # followed by the mask. They are separate physical forms, not ordinary IORs.
    for op in model["operations"]:
        if op["name"] not in {"TADD", "TMUL", "TSHR", "MGATHER"}:
            continue
        for base in list(op["forms"]):
            f = deepcopy(base)
            f["id"] += "-exec-mask"
            f["encoding"] = "E" + str(len(op["forms"]))
            f["configs"] += [
                config("PredInv", "DATR", "PredInv", "0"),
                config("ZeroMask", "DATR", "ZeroMask", "0"),
            ]
            ior = [c for c in f["commands"] if c[0] == "IOR"]
            if len(ior) > 1:
                continue
            if ior:
                command = ior[0]
                command[0] = "ExecMask"
            else:
                command = ["ExecMask", len(f["commands"]), []]
                f["commands"].append(command)
            source_count = len(command[2])
            if source_count >= 3:
                continue
            index = len(f["bindings"])
            f["bindings"].append(
                [
                    "ExecMask",
                    "ExecMask",
                    "GPRSource",
                    [["RegSrc" + str(source_count), None, None, False, 0]],
                ]
            )
            command[2].append([index, 0])
            op["forms"].append(f)


def add_tile_execution_mask_forms(model):
    for op in model["operations"]:
        if op["name"] not in {"TADD", "TSUB", "TMUL", "TNEG", "TCMP", "TCMPS",
                              "TAND", "TOR", "TXOR"}:
            continue
        for base in list(op["forms"]):
            if base.get("encoding") or len(base["commands"]) != 1:
                continue
            command = base["commands"][0]
            if command[0] != "IOT":
                continue
            sources = [pair for pair in command[2]
                       if base["bindings"][pair[0]][2] == "LocalSource"]
            destinations = [pair for pair in command[2]
                            if base["bindings"][pair[0]][2] == "LocalDestination"]
            if not sources or len(sources) > 2 or len(destinations) != 1:
                continue
            if len(sources) + len(destinations) != len(command[2]):
                continue
            masked = deepcopy(base)
            masked["id"] += "-tile-exec-mask"
            masked["encoding"] = "E" + str(len(op["forms"]))
            fields = {item[0] for item in masked["configs"]}
            for field in ("PredInv", "ZeroMask"):
                if field not in fields:
                    masked["configs"].append(config(field, "DATR", field, "0"))
            mask = len(masked["bindings"])
            masked["bindings"].append(
                ["ExecMask", "ExecMask", "LocalSource",
                 [["Role0", None, None, False, 1]]])
            destination = destinations[0]
            masked["bindings"][destination[0]][3][destination[1]][0] = "Role1"
            masked["commands"] = [
                ["IOT", 0, sources],
                ["IOT", 1, [[mask, 0], destination]],
            ]
            op["forms"].append(masked)


def render(model):
    tables = {
        k: []
        for k in [
            "Operations",
            "Forms",
            "Configs",
            "ConfigTargets",
            "Bindings",
            "BindingMembers",
            "Commands",
            "CommandMembers",
        ]
    }
    for oi, op in enumerate(model["operations"]):
        tables["Operations"].append(
            [
                op["name"],
                ("HeaderKind", op["family"]),
                op["selector"],
                len(tables["Forms"]),
                len(op["forms"]),
            ]
        )
        for f in op["forms"]:
            ci, bi, cmdi = (len(tables[n]) for n in ["Configs", "Bindings", "Commands"])
            for c in f["configs"]:
                ti = len(tables["ConfigTargets"])
                tables["ConfigTargets"].extend(
                    [[("TargetKind", t[0]), ("TargetSlot", t[1]), t[2]] for t in c[8]]
                )
                tables["Configs"].append(c[:8] + [ti, len(c[8])])
            members = {}
            for bidx, b in enumerate(f["bindings"]):
                mi = len(tables["BindingMembers"])
                for midx, m in enumerate(b[3]):
                    members[bidx, midx] = len(tables["BindingMembers"])
                    tables["BindingMembers"].append([("MemberSlot", m[0])] + m[1:])
                tables["Bindings"].append(
                    b[:2] + [("BindingKind", b[2]), mi, len(b[3])]
                )
            for c in f["commands"]:
                mi = len(tables["CommandMembers"])
                tables["CommandMembers"].extend([[members[tuple(m)]] for m in c[2]])
                tables["Commands"].append([("CommandKind", c[0]), c[1], mi, len(c[2])])
            tables["Forms"].append(
                [
                    f["spelling"],
                    f["format"],
                    f["id"],
                    oi,
                    ("ShapeKind", f["shape"]),
                    f["canonical"],
                    ci,
                    len(f["configs"]),
                    bi,
                    len(f["bindings"]),
                    cmdi,
                    len(f["commands"]),
                    f.get("encoding"),
                ]
            )
    # Keep declarations in the generator; one schema defines the table layout.
    generator = Path(__file__).with_name("generate_tile_macro_catalog.py").read_text()
    declarations = generator.split("enum class TileMacroHeaderKind", 1)[1].split(
        "static constexpr StringLiteral", 1
    )[0]
    declarations = "enum class TileMacroHeaderKind" + declarations
    declarations = declarations.replace("{{", "{").replace("}}", "}")

    def val(v):
        if isinstance(v, tuple):
            return "TileMacro" + v[0] + "::" + v[1]
        if v is None:
            return "nullptr"
        if isinstance(v, bool):
            return str(v).lower()
        return json.dumps(v)

    digest = hashlib.sha256(
        json.dumps(model, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()
    out = [
        "// Generated by llvm/utils/linxv5/generate_tile_macro_catalog.py.",
        "// Source architecture: " + model["architecture"],
        "// Source SHA-256: " + digest,
        "// DO NOT EDIT.",
        "",
        declarations,
        'static constexpr StringLiteral TileMacroArchitectureIdentity = "'
        + model["architecture"]
        + '";',
        'static constexpr StringLiteral TileMacroCatalogSHA256 = "' + digest + '";',
        f'static constexpr unsigned TileMacroOperationCount = {len(tables["Operations"])};',
        f'static constexpr unsigned TileMacroFormCount = {len(tables["Forms"])};',
        "static constexpr unsigned TileMacroMnemonicColumn = 18;",
        "",
    ]
    for name, rows in tables.items():
        typ = {
            "Operations": "OperationDesc",
            "Forms": "FormDesc",
            "Configs": "ConfigDesc",
            "ConfigTargets": "ConfigTargetDesc",
            "Bindings": "BindingDesc",
            "BindingMembers": "BindingMemberDesc",
            "Commands": "CommandDesc",
            "CommandMembers": "CommandMemberDesc",
        }[name]
        out.append("static constexpr TileMacro" + typ + " TileMacro" + name + "[] = {")
        out.extend("  {" + ", ".join(val(v) for v in row) + "}," for row in rows)
        out.extend(["};", ""])
    return "\n".join(out)
