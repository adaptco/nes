import React from "react";
import HashBadge from "../components/HashBadge";
import KeyValueTable from "../components/KeyValueTable";
import { sha256Hex } from "../hashing";
import { canonicalJson } from "../jcs";
import { TelemetryEventV1 } from "../types";

export default function EventDetail({ event }: { event: TelemetryEventV1 }) {
  const [computed, setComputed] = React.useState("");

  React.useEffect(() => {
    const next = { ...event } as Record<string, unknown>;
    delete next.canonicalHash;
    sha256Hex(canonicalJson(next)).then(setComputed);
  }, [event]);

  const snapshot = event.payload?.snapshot ?? {};
  const caseId = event.lineage?.caseId ?? event.payload?.caseId ?? "unknown";

  return (
    <div style={{ padding: 16 }}>
      <h2>Case: {caseId}</h2>
      <HashBadge label="Canonical Hash" value={event.canonicalHash} />
      <HashBadge label="Computed Hash" value={computed || "…"} />
      <div style={{ margin: "8px 0 16px" }}>
        Verification: {computed ? (computed === event.canonicalHash ? "PASS" : "FAIL") : "PENDING"}
      </div>
      <h3>Snapshot</h3>
      <KeyValueTable
        rows={[
          ["sensorBuffer.sha256", snapshot.sensorBuffer?.sha256],
          ["environmentContext.sha256", snapshot.environmentContext?.sha256],
          ["macroTexture.sha256", snapshot.macroTexture?.sha256],
        ]}
      />
    </div>
  );
}
