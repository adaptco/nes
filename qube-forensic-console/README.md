# Qube Forensic Console

Minimal end-to-end scaffold for a loader-consumable forensic console:

- **Schemas**: forensic report + telemetry envelope (Draft 2020-12)
- **Ingest**: validate report, enforce invariants, compute canonical hash, append NDJSON SSOT
- **Console UI**: browse telemetry and verify hash integrity client-side

## Frozen invariants

- Embedding dimension: `1536`
- Seal phrase: `Canonical truth, attested and replayable.`
- Hash algorithm: `SHA-256`

## Quickstart

### Python ingest

```bash
cd qube-forensic-console
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

Ingest a report into `telemetry_store/ssot_telemetry_audit.ndjson`:

```python
from pathlib import Path
from qube_forensics.ingest import IngestConfig, ingest_forensic_report, load_report_json

cfg = IngestConfig(
    forensic_schema_path=Path("schemas/forensics/qube_forensic_report.v1.schema.json"),
    telemetry_schema_path=Path("schemas/telemetry/telemetry_event.v1.schema.json"),
    telemetry_store_path=Path("telemetry_store/ssot_telemetry_audit.ndjson"),
)

report = load_report_json(Path("report.json"))
event = ingest_forensic_report(report, session_id="sess_001", cfg=cfg)
print(event["canonicalHash"])
```

Copy SSOT data to `console/public/ssot_telemetry_audit.ndjson` for UI viewing.

### Console UI

```bash
cd console
npm i
npm run dev
```

Open <http://localhost:5173>.
