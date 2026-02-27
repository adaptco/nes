from dataclasses import dataclass
from pathlib import Path
import json
from typing import Any

from .emit_ndjson import append_ndjson
from .hashing import sha256_hex_from_obj
from .validate import load_schema, validate_or_raise

SEAL_PHRASE = "Canonical truth, attested and replayable."
EMBED_DIM_CANON = 1536


@dataclass(frozen=True)
class IngestConfig:
    forensic_schema_path: Path
    telemetry_schema_path: Path
    telemetry_store_path: Path
    source_system: str = "QUBE_FORENSICS"
    payload_type: str = "qube_forensic_report.v1"


def ingest_forensic_report(report: dict[str, Any], *, session_id: str, cfg: IngestConfig) -> dict[str, Any]:
    validate_or_raise(report, load_schema(cfg.forensic_schema_path))

    if report.get("embeddingDimension") != EMBED_DIM_CANON:
        raise ValueError(f"embeddingDimension must be {EMBED_DIM_CANON}")
    if report.get("governance", {}).get("sealPhrase") != SEAL_PHRASE:
        raise ValueError("sealPhrase mismatch")

    case_id = report["caseId"]
    event = {
        "schemaVersion": "telemetry.event.v1",
        "eventKey": f"forensic.report.ingested::{case_id}",
        "sessionId": session_id,
        "timestamp": report["timestamp"],
        "sourceSystem": cfg.source_system,
        "payloadType": cfg.payload_type,
        "payload": report,
        "sealPhrase": SEAL_PHRASE,
        "lineage": {
            "caseId": case_id,
            "kernel": report.get("kernel"),
            "computeNode": report.get("computeNode", {}).get("model"),
        },
    }

    event["canonicalHash"] = sha256_hex_from_obj(event)
    validate_or_raise(event, load_schema(cfg.telemetry_schema_path))
    append_ndjson(cfg.telemetry_store_path, event)
    return event


def load_report_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))
