#!/usr/bin/env python3
"""Generate the exact PTO 0.58.6 TileOp macro schema used by LLVM MC."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


EXPECTED_ARCHITECTURE = "0.58.6"
EXPECTED_OPERATIONS = 117
EXPECTED_FORMS = 142


def cxx_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def cxx_optional(value: object) -> str:
    return "nullptr" if value is None else cxx_string(str(value))


def selector_value(value: str | int) -> int:
    return int(value, 16) if isinstance(value, str) else int(value)


def header_selector(header: dict) -> int:
    value = header.get("selector")
    return selector_value(value if value is not None else header["function"])


def header_kind(engine: str, command: str) -> str:
    if engine in {"VEC", "SFU"} and command in {"BSTART.VEC", "BSTART.SFU"}:
        return "TEPL"
    if engine == "TLSU" and command == "BSTART.GMOV":
        return "GMOV"
    if engine == "TLSU":
        return "TLSU"
    if engine == "CUBE":
        return "CUBE"
    raise ValueError(f"unsupported TileOp header command: {command}")


def shape_kind(form: dict) -> str:
    fields = tuple(
        item["field"]
        for item in form["configuration"]
        if item.get("configuration_kind") == "dimension"
    )
    shapes = {
        ("Row", "Col", "ValidRow", "ValidCol"): "Rectangular",
        ("M", "N", "K"): "Matrix",
        ("ValidCol",): "ValidCol",
        ("ValidK", "ValidN", "TotalK"): "Weight",
        (): "None",
    }
    if fields not in shapes:
        raise ValueError(f"unsupported dimension signature: {fields}")
    return shapes[fields]


def binding_kind(binding: dict) -> str:
    kinds = {
        "tile-source-binding": "LocalSource",
        "tile-destination-binding": "LocalDestination",
        "shared-tile-binding": (
            "SharedDestination"
            if binding["role_kind"] == "destination"
            else "SharedSource"
        ),
        "scalar-binding": (
            "GPRDestination"
            if binding["role_kind"] == "destination"
            else ("GPRListSource" if binding["syntax"].startswith("[") else "GPRSource")
        ),
        "predicate-tile-source": "PredicateTileSource",
        "predicate-tile-destination": "PredicateTileDestination",
        "predicate-cell-source": "PredicateCellSource",
        "predicate-cell-destination": "PredicateCellDestination",
        "predicate-gpr-source": "GPRSource",
        "predicate-gpr-destination": "GPRDestination",
    }
    try:
        return kinds[binding["binding_kind"]]
    except KeyError as error:
        raise ValueError(f"unsupported operand binding: {binding['binding_kind']}") from error


def target_kind(command: str) -> str:
    if command.startswith("BSTART."):
        return "Header"
    kinds = {
        "B.DATR": "DATR",
        "B.FPATR": "FPATR",
        "B.DIM": "DIM",
        "B.IOT": "IOT",
        "B.IOS": "IOS",
    }
    try:
        return kinds[command]
    except KeyError as error:
        raise ValueError(f"unsupported configuration target: {command}") from error


def target_slot(slot: str) -> str:
    slots = {
        "DataType": "DataType",
        "Layout": "Layout",
        "Canonicalize": "Canonicalize",
        "PadValueOrByteId": "PadValueOrByteId",
        "CMode": "CMode",
        "RMode": "RMode",
        "Sat": "Sat",
        "PreQuantMode": "PreQuantMode",
        "ReluMode": "ReluMode",
        "GroupNCode": "GroupNCode",
        "RowMaxEn": "RowMaxEn",
        "GroupMaxEn": "GroupMaxEn",
        "RowMaxInit": "RowMaxInit",
        "MaxAbsEn": "MaxAbsEn",
        "TransA": "TransA",
        "TransB": "TransB",
        "CScaleEn": "CScaleEn",
        "LB0": "LB0",
        "LB1": "LB1",
        "LB2": "LB2",
        "PE_MASK": "PEMask",
    }
    try:
        return slots[slot]
    except KeyError as error:
        raise ValueError(f"unsupported configuration slot: {slot}") from error


def command_kind(command: str) -> str:
    return {"B.IOT": "IOT", "B.IOS": "IOS", "B.IOR": "IOR"}[command]


def member_slot(slot: str) -> str:
    return {
        "Role0": "Role0",
        "Role1": "Role1",
        "Role2": "Role2",
        "RegDst": "RegDst",
        "RegSrc0": "RegSrc0",
        "RegSrc1": "RegSrc1",
        "RegSrc2": "RegSrc2",
        "RegSrc3": "RegSrc3",
    }[slot]


def generate(catalog_path: Path) -> str:
    raw = catalog_path.read_bytes()
    catalog = json.loads(raw)
    if catalog["architecture_identity"] != EXPECTED_ARCHITECTURE:
        raise ValueError(
            f"expected PTO {EXPECTED_ARCHITECTURE}, found {catalog['architecture_identity']}"
        )
    operations = catalog["operations"]
    forms = [(operation, form) for operation in operations for form in operation["forms"]]
    if len(operations) != EXPECTED_OPERATIONS:
        raise ValueError(f"expected {EXPECTED_OPERATIONS} operations, found {len(operations)}")
    if len(forms) != EXPECTED_FORMS:
        raise ValueError(f"expected {EXPECTED_FORMS} forms, found {len(forms)}")

    operation_rows: list[str] = []
    form_rows: list[str] = []
    config_rows: list[str] = []
    target_rows: list[str] = []
    binding_rows: list[str] = []
    binding_member_rows: list[str] = []
    command_rows: list[str] = []
    command_member_rows: list[str] = []
    first_form = 0
    header_signatures: set[tuple[str, int]] = set()

    for operation_index, operation in enumerate(operations):
        operation_forms = operation["forms"]
        signatures = {
            (
                header_kind(operation["engine"], form["expansion"]["header"]["command"]),
                header_selector(form["expansion"]["header"]),
            )
            for form in operation_forms
        }
        if len(signatures) != 1:
            raise ValueError(f"{operation['mnemonic']} has multiple header signatures")
        kind, selector = signatures.pop()
        if (kind, selector) in header_signatures:
            raise ValueError(f"duplicate physical header signature: {(kind, selector)}")
        header_signatures.add((kind, selector))
        operation_rows.append(
            f"  {{{cxx_string(operation['mnemonic'])}, TileMacroHeaderKind::{kind}, "
            f"0x{selector:x}, {first_form}, {len(operation_forms)}}},"
        )

        for form in operation_forms:
            first_config = len(config_rows)
            first_binding = len(binding_rows)
            first_command = len(command_rows)

            configs = form["configuration"]
            config_bindings = form["expansion"]["configuration_bindings"]
            if len(configs) != len(config_bindings):
                raise ValueError(f"{form['spelling']}: configuration/binding length mismatch")
            for config, expansion in zip(configs, config_bindings):
                if config["field"] != expansion["field"]:
                    raise ValueError(f"{form['spelling']}: configuration field mismatch")
                first_target = len(target_rows)
                for target in expansion["targets"]:
                    group = -1 if target.get("group") is None else int(target["group"])
                    target_rows.append(
                        f"  {{TileMacroTargetKind::{target_kind(target['command'])}, "
                        f"TileMacroTargetSlot::{target_slot(target['slot'])}, {group}}},"
                    )
                config_rows.append(
                    f"  {{{cxx_string(config['field'])}, {cxx_string(config['syntax'])}, "
                    f"{cxx_optional(config.get('default'))}, {cxx_optional(config.get('constraint'))}, "
                    f"{cxx_optional(expansion.get('resolution', {}).get('kind'))}, "
                    f"{cxx_optional(expansion.get('resolution', {}).get('expression'))}, "
                    f"{str(config.get('configuration_kind') == 'dimension').lower()}, "
                    f"{str(config.get('optional', False)).lower()}, {first_target}, "
                    f"{len(expansion['targets'])}}},"
                )

            member_lookup: dict[tuple[str, int, str], int] = {}
            for binding in form["expansion"]["operand_bindings"]:
                first_member = len(binding_member_rows)
                for member in binding["members"]:
                    flags = 0
                    for modifier in member.get("eligible_modifiers", []):
                        name = modifier["modifier"]
                        flags |= 1 if name == "B.SUBVIEW" else 2 if name.startswith("B.ASSEMBLE") else 0
                    absolute_member = len(binding_member_rows)
                    key = (binding["field"], int(member["command_group"]), member["slot"])
                    if key in member_lookup:
                        raise ValueError(f"{form['spelling']}: duplicate binding member {key}")
                    member_lookup[key] = absolute_member
                    binding_member_rows.append(
                        f"  {{TileMacroMemberSlot::{member_slot(member['slot'])}, "
                        f"{cxx_optional(member.get('condition'))}, {cxx_optional(member.get('default'))}, "
                        f"{str(member.get('optional', False)).lower()}, {flags}}},"
                    )
                binding_rows.append(
                    f"  {{{cxx_string(binding['field'])}, {cxx_string(binding['syntax'])}, "
                    f"TileMacroBindingKind::{binding_kind(binding)}, {first_member}, "
                    f"{len(binding['members'])}}},"
                )

            for command in form["expansion"]["operand_commands"]:
                first_member = len(command_member_rows)
                for member in command["members"]:
                    key = (member["field"], int(command["group"]), member["slot"])
                    if key not in member_lookup:
                        raise ValueError(f"{form['spelling']}: missing operand binding for {key}")
                    command_member_rows.append(f"  {{{member_lookup[key]}}},")
                command_rows.append(
                    f"  {{TileMacroCommandKind::{command_kind(command['command'])}, "
                    f"{int(command['group'])}, {first_member}, {len(command['members'])}}},"
                )

            fold = form["expansion"]["fold"]
            form_rows.append(
                f"  {{{cxx_string(form['spelling'])}, {cxx_string(form['macro_format'])}, "
                f"{cxx_string(form['expansion']['form_id'])}, {operation_index}, "
                f"TileMacroShapeKind::{shape_kind(form)}, "
                f"{str(bool(fold['unique_without_runtime_state'])).lower()}, "
                f"{first_config}, {len(configs)}, {first_binding}, "
                f"{len(form['expansion']['operand_bindings'])}, {first_command}, "
                f"{len(form['expansion']['operand_commands'])}}},"
            )
        first_form += len(operation_forms)

    digest = hashlib.sha256(raw).hexdigest()
    mnemonic_column = max(len(form["spelling"]) for _, form in forms) + 2
    return f"""// Generated by llvm/utils/linxv5/generate_tile_macro_catalog.py.
// Source architecture: {EXPECTED_ARCHITECTURE}
// Source SHA-256: {digest}
// DO NOT EDIT.

enum class TileMacroHeaderKind : uint8_t {{ TEPL, TLSU, CUBE, GMOV }};
enum class TileMacroShapeKind : uint8_t {{ None, Rectangular, Matrix, ValidCol, Weight }};
enum class TileMacroBindingKind : uint8_t {{
  LocalSource, LocalDestination, SharedSource, SharedDestination,
  GPRSource, GPRListSource, GPRDestination, PredicateTileSource,
  PredicateTileDestination, PredicateCellSource, PredicateCellDestination
}};
enum class TileMacroCommandKind : uint8_t {{ IOT, IOS, IOR }};
enum class TileMacroMemberSlot : uint8_t {{
  Role0, Role1, Role2, RegDst, RegSrc0, RegSrc1, RegSrc2, RegSrc3
}};
enum class TileMacroTargetKind : uint8_t {{ Header, DATR, FPATR, DIM, IOT, IOS }};
enum class TileMacroTargetSlot : uint8_t {{
  DataType, Layout, Canonicalize, PadValueOrByteId, CMode, RMode, Sat,
  PreQuantMode, ReluMode, GroupNCode, RowMaxEn, GroupMaxEn, RowMaxInit,
  MaxAbsEn, TransA, TransB, CScaleEn, LB0, LB1, LB2, PEMask
}};

struct TileMacroOperationDesc {{
  const char *Mnemonic;
  TileMacroHeaderKind HeaderKind;
  uint16_t Selector;
  uint16_t FirstForm;
  uint8_t NumForms;
}};
struct TileMacroFormDesc {{
  const char *Spelling;
  const char *MacroFormat;
  const char *FormID;
  uint16_t Operation;
  TileMacroShapeKind ShapeKind;
  bool UniqueWithoutRuntimeState;
  uint16_t FirstConfig;
  uint8_t NumConfigs;
  uint16_t FirstBinding;
  uint8_t NumBindings;
  uint16_t FirstCommand;
  uint8_t NumCommands;
}};
struct TileMacroConfigDesc {{
  const char *Field;
  const char *Syntax;
  const char *Default;
  const char *Constraint;
  const char *ResolutionKind;
  const char *ResolutionExpression;
  bool IsDimension;
  bool Optional;
  uint16_t FirstTarget;
  uint8_t NumTargets;
}};
struct TileMacroConfigTargetDesc {{
  TileMacroTargetKind Kind;
  TileMacroTargetSlot Slot;
  int8_t Group;
}};
struct TileMacroBindingDesc {{
  const char *Field;
  const char *Syntax;
  TileMacroBindingKind Kind;
  uint16_t FirstMember;
  uint8_t NumMembers;
}};
struct TileMacroBindingMemberDesc {{
  TileMacroMemberSlot Slot;
  const char *Condition;
  const char *Default;
  bool Optional;
  uint8_t ModifierFlags;
}};
struct TileMacroCommandDesc {{
  TileMacroCommandKind Kind;
  uint8_t Group;
  uint16_t FirstMember;
  uint8_t NumMembers;
}};
struct TileMacroCommandMemberDesc {{ uint16_t BindingMember; }};

static constexpr StringLiteral TileMacroArchitectureIdentity = "{EXPECTED_ARCHITECTURE}";
static constexpr StringLiteral TileMacroCatalogSHA256 = "{digest}";
static constexpr unsigned TileMacroOperationCount = {len(operations)};
static constexpr unsigned TileMacroFormCount = {len(forms)};
static constexpr unsigned TileMacroMnemonicColumn = {mnemonic_column};

static constexpr TileMacroOperationDesc TileMacroOperations[] = {{
{chr(10).join(operation_rows)}
}};
static constexpr TileMacroFormDesc TileMacroForms[] = {{
{chr(10).join(form_rows)}
}};
static constexpr TileMacroConfigDesc TileMacroConfigs[] = {{
{chr(10).join(config_rows)}
}};
static constexpr TileMacroConfigTargetDesc TileMacroConfigTargets[] = {{
{chr(10).join(target_rows)}
}};
static constexpr TileMacroBindingDesc TileMacroBindings[] = {{
{chr(10).join(binding_rows)}
}};
static constexpr TileMacroBindingMemberDesc TileMacroBindingMembers[] = {{
{chr(10).join(binding_member_rows)}
}};
static constexpr TileMacroCommandDesc TileMacroCommands[] = {{
{chr(10).join(command_rows)}
}};
static constexpr TileMacroCommandMemberDesc TileMacroCommandMembers[] = {{
{chr(10).join(command_member_rows)}
}};

static_assert(sizeof(TileMacroOperations) / sizeof(TileMacroOperations[0]) == TileMacroOperationCount,
              "incomplete PTO TileOp operation catalog");
static_assert(sizeof(TileMacroForms) / sizeof(TileMacroForms[0]) == TileMacroFormCount,
              "incomplete PTO TileOp form catalog");
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = generate(args.catalog)
    if args.check:
        current = args.output.read_text() if args.output.exists() else ""
        if current != generated:
            raise SystemExit(f"stale generated TileOp catalog: {args.output}")
        return
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generated)


if __name__ == "__main__":
    main()
