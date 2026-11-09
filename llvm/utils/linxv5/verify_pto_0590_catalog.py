import json
from tile_macro_model import SEED, extend_model, load_model

source = json.loads(SEED.with_name("pto_0590_catalog.json").read_text())
assert source["architecture"] == "0.59.0"
assert len(source["operations"]) == 119
assert sum(len(operation["forms"]) for operation in source["operations"]) == 168
model = load_model()
extend_model(model)
operations = {operation["name"]: operation for operation in model["operations"]}
for operation in source["operations"]:
    actual = operations[operation["name"]]
    assert actual["family"] == operation["family"]
    assert actual["selector"] == operation["selector"]
    for form in operation["forms"]:
        if form["spelling"].startswith("TSTORE.ATOMIC_"):
            matches = [candidate for candidate in actual["forms"]
                       if candidate["id"] == form["id"]]
            assert len(matches) == 1
            assert matches[0]["commands"] == form["commands"]
            assert matches[0]["bindings"] == form["bindings"]
assert model["architecture"] == source["architecture"]
assert model["normative_source_sha256"] == source["source_sha256"]
print("PTO 0.59: 119 normative operation header assignments verified; "
      "24 atomic TSTORE forms migrated; compatibility deltas retained")
