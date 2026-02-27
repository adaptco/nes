import json
from pathlib import Path

from jsonschema import Draft202012Validator


def load_schema(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_or_raise(payload: dict, schema: dict) -> None:
    validator = Draft202012Validator(schema)
    errors = sorted(validator.iter_errors(payload), key=lambda e: tuple(str(p) for p in e.path))
    if not errors:
        return
    preview = []
    for err in errors[:10]:
        field = "/".join(str(p) for p in err.path) or "<root>"
        preview.append(f"{field}: {err.message}")
    raise ValueError("Schema validation failed: " + " | ".join(preview))
