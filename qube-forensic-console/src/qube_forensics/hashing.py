import hashlib
from typing import Any

from .jcs import canonical_json


def sha256_hex_from_obj(obj: Any) -> str:
    return hashlib.sha256(canonical_json(obj).encode("utf-8")).hexdigest()
