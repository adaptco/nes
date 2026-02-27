import React from "react";
import { TelemetryEventV1 } from "../types";

export default function EventList({
  events,
  selected,
  onSelect,
}: {
  events: TelemetryEventV1[];
  selected: TelemetryEventV1 | null;
  onSelect: (e: TelemetryEventV1) => void;
}) {
  return (
    <div style={{ padding: 12 }}>
      <div style={{ fontWeight: 700, marginBottom: 8 }}>Forensic Telemetry</div>
      {events.map((event) => {
        const active = selected?.canonicalHash === event.canonicalHash;
        const caseId = event.lineage?.caseId ?? event.payload?.caseId ?? "unknown";
        return (
          <button
            key={event.canonicalHash}
            onClick={() => onSelect(event)}
            style={{
              width: "100%",
              textAlign: "left",
              padding: 10,
              border: "1px solid #ddd",
              marginBottom: 8,
              background: active ? "#f5f5f5" : "#fff",
              cursor: "pointer",
            }}
          >
            <div style={{ fontWeight: 600 }}>{caseId}</div>
            <div style={{ fontSize: 12, opacity: 0.8 }}>{event.timestamp}</div>
          </button>
        );
      })}
    </div>
  );
}
