import argparse
import hashlib
import json
from pathlib import Path

import generate_tile_macro_catalog as generator


def project(source):
    catalog = json.loads(source.read_bytes())
    if catalog["architecture_identity"] != "0.59.0":
        raise ValueError("expected the pinned PTO 0.59.0 catalog")
    if len(catalog["operations"]) != 119:
        raise ValueError("expected 119 normative operations")
    operations = []
    for operation in catalog["operations"]:
        forms = []
        for form in operation["forms"]:
            expansion = form["expansion"]
            configs = []
            if len(form["configuration"]) != len(expansion["configuration_bindings"]):
                raise ValueError("configuration binding length mismatch")
            for config, binding in zip(form["configuration"], expansion["configuration_bindings"]):
                if config["field"] != binding["field"]:
                    raise ValueError("configuration binding field mismatch")
                resolution = binding.get("resolution", {})
                configs.append([
                    config["field"], config["syntax"], config.get("default"),
                    config.get("constraint"), resolution.get("kind"),
                    resolution.get("expression"),
                    config.get("configuration_kind") == "dimension",
                    config.get("optional", False),
                    [[generator.target_kind(target["command"]),
                      generator.target_slot(target["slot"]),
                      -1 if target.get("group") is None else int(target["group"])]
                     for target in binding["targets"]],
                ])
            bindings = []
            members = {}
            for binding_index, binding in enumerate(expansion["operand_bindings"]):
                projected_members = []
                for member_index, member in enumerate(binding["members"]):
                    flags = 0
                    for modifier in member.get("eligible_modifiers", []):
                        name = modifier["modifier"]
                        flags |= (1 if name == "B.SUBVIEW" else
                                  4 if name == "B.ASSEMBLE MIDDLE" else
                                  8 if name == "B.ASSEMBLE LAST" else
                                  2 if name.startswith("B.ASSEMBLE") else 0)
                    projected_members.append([
                        generator.member_slot(member["slot"]), member.get("condition"),
                        member.get("default"), member.get("optional", False), flags,
                    ])
                    members[binding["field"], int(member["command_group"]), member["slot"]] = [binding_index, member_index]
                bindings.append([binding["field"], binding["syntax"],
                                 generator.binding_kind(binding), projected_members])
            commands = [[generator.command_kind(command["command"]), int(command["group"]),
                         [members[member["field"], int(command["group"]), member["slot"]]
                          for member in command["members"]]]
                        for command in expansion["operand_commands"]]
            forms.append(dict(spelling=form["spelling"], format=form["macro_format"],
                              id=expansion["form_id"], shape=generator.shape_kind(form),
                              canonical=bool(expansion["fold"]["canonical_without_runtime_state"]),
                              configs=configs, bindings=bindings, commands=commands))
        header = operation["forms"][0]["expansion"]["header"]
        operations.append(dict(name=operation["mnemonic"],
                               family=generator.header_kind(operation["engine"], header["command"]),
                               selector=generator.header_selector(header), forms=forms))
    return dict(architecture=catalog["architecture_identity"],
                source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                operations=operations)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    contents = json.dumps(project(args.catalog), indent=2) + "\n"
    if args.check:
        if args.output.read_text() != contents:
            raise SystemExit("stale PTO source catalog projection")
    else:
        args.output.write_text(contents)


if __name__ == "__main__":
    main()
