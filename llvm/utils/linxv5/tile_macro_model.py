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
