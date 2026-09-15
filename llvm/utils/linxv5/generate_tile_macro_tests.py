#!/usr/bin/env python3
"""Generate one valid default-oriented macro case for every PTO 0.58.6 form."""

import json
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: generate_tile_macro_tests.py CATALOG OUTPUT")
catalog = json.load(open(sys.argv[1]))
out = [
    "# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o",
    "# RUN: llvm-objdump -d --no-show-raw-insn %t.o > %t.diss",
    "# RUN: FileCheck %s < %t.diss",
    "# RUN: %python %S/../../../utils/linxv5/verify_tile_macro_forms.py %s %t.diss",
    "# RUN: %python %S/../../../utils/linxv5/roundtrip_tile_macro_forms.py llvm-mc llvm-objdump %s",
    "# RUN: %python %S/../../../utils/linxv5/verify_tile_macro_required_operands.py llvm-mc %s",
    "# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=PHYSICAL",
    ".text",
]
tile_index = gpr_index = shared_index = 0

def gpr_label(field):
    return {
        "RowStrideGPR": "stride",
    }.get(field)

def gpr_list_label(member):
    syntax = member["syntax"]
    if "BaseGPR" in syntax or "GMBaseGPR" in syntax:
        return "base"
    if "RowStrideGPR" in syntax:
        return "stride"
    if "ShapeGPR" in syntax:
        return "shape"
    if "StartGPR" in syntax:
        return "start"
    return None

def attribute(field, spelling):
    if field == "DataType" and spelling == "TCI":
        return "U32"
    return {
        "DataType": "FP32", "SrcDataType": "FP32", "DstDataType": "FP16",
        "ValueDataType": "FP32", "AType": "FP32", "BType": "FP32",
        "U8": "U8", "U32": "U32", "DTYPE_NONE": "DTYPE_NONE",
        "CubeLayout": "ND2M32" if spelling.startswith("TLOAD") else "M322ND",
        "WeightLayout": "OHWI2NK", "PadValue": "Null",
    }.get(field)

for operation in catalog["operations"]:
    for form in operation["forms"]:
        config = []
        dims = [c["field"] for c in form["configuration"]
                if c.get("configuration_kind") == "dimension"]
        if dims == ["Row", "Col", "ValidRow", "ValidCol"]:
            if any(c["field"] == "CubeLayout" for c in form["configuration"]):
                config.extend(["Row=1", "Col=32"])
            elif form["spelling"] == "TGPR2T":
                config.extend(["Row=32", "Col=4"])
            else:
                row = "128" if any(c["field"] == "U8" for c in form["configuration"]) else "32"
                config.extend([f"Row={row}", "Col=1"])
        elif dims == ["ValidRow", "ValidCol"]:
            config.extend(["ValidRow=32", "ValidCol=8"])
        elif dims == ["M", "N", "K"]:
            if form["spelling"].startswith("TGEMV"):
                config.extend(["M=1", "N=8", "K=16"])
            else:
                config.extend(["M=2", "N=3", "K=4"])
        elif dims == ["ValidCol"]:
            config.append("ValidCol=1")
        elif dims == ["ValidK", "ValidN", "TotalK"]:
            config.extend(["ValidK=16", "ValidN=8", "TotalK=32"])
        for item in form["configuration"]:
            if item.get("configuration_kind") != "attribute":
                continue
            value = attribute(item["field"], form["spelling"])
            if value is not None and not item.get("optional"):
                config.append(value)

        operands = []
        for binding in form["expansion"]["operand_bindings"]:
            members = binding["members"]
            if all(member.get("condition") for member in members):
                continue
            required = [m for m in members if not m.get("optional") and m.get("default") is None]
            show_optional_gpr = (
                binding["binding_kind"] == "scalar-binding"
                and any(not member.get("condition") for member in members)
            )
            if (not show_optional_gpr and not required
                    and all(m.get("optional") or m.get("default") is not None
                            for m in members)):
                continue
            kind = binding["binding_kind"]
            destination = binding["role_kind"] == "destination"
            if binding["syntax"].startswith("["):
                regs = []
                for member in members:
                    if member.get("condition"):
                        continue
                    label = gpr_list_label(member)
                    reg = f"a{len(regs)}"
                    regs.append(f"{label}={reg}" if label else reg)
                operands.append("[" + ", ".join(regs) + "]")
                continue
            if kind == "shared-tile-binding":
                value = f"S{shared_index % 8}"
                shared_index += 1
            elif kind in {"scalar-binding", "predicate-gpr-source",
                          "predicate-gpr-destination"}:
                value = f"a{gpr_index % 8}"
                gpr_index += 1
            else:
                prefix = "U" if "predicate-tile" in kind else "M" if "predicate-cell" in kind else "T"
                value = prefix if destination else f"{prefix}#1"
                tile_index += 1
            if destination:
                value = "->" + value
                if kind != "predicate-gpr-destination":
                    value += "<128B>"
            label = gpr_label(binding["field"])
            if label and kind in {
                "scalar-binding", "predicate-gpr-source",
                "predicate-gpr-destination",
            }:
                value = value.replace("->", f"->{label}=", 1) if destination else f"{label}={value}"
            operands.append(value)
        line = f"{form['spelling']} <{', '.join(config)}>"
        if operands:
            line += ", " + ", ".join(operands)
        canonical = form["expansion"]["fold"]["canonical_without_runtime_state"]
        out.append(
            f"# FORM: fold=1 canonical={int(canonical)} spelling={form['spelling']} operation={operation['mnemonic']}"
        )
        out.append(f"# CHECK: {form['spelling']}{{{{ +}}}}<")
        out.append(line)
out.extend([
    "# PHYSICAL: BSTART",
    "# PHYSICAL-NOT: C.B.DIMI 1,",
])
open(sys.argv[2], "w").write("\n".join(out) + "\n")
