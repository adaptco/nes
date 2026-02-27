import React from "react";
import { loadTelemetryNDJSON } from "./api";
import { TelemetryEventV1 } from "./types";
import EventList from "./views/EventList";
import EventDetail from "./views/EventDetail";

export default function App() {
  const [events, setEvents] = React.useState<TelemetryEventV1[]>([]);
  const [selected, setSelected] = React.useState<TelemetryEventV1 | null>(null);

  React.useEffect(() => {
    loadTelemetryNDJSON("/ssot_telemetry_audit.ndjson").then((items) => {
      const filtered = items.filter((item) => item.payloadType === "qube_forensic_report.v1");
      setEvents(filtered);
      setSelected(filtered[0] ?? null);
    });
  }, []);

  return (
    <div style={{ display: "grid", gridTemplateColumns: "320px 1fr", height: "100vh" }}>
      <div style={{ borderRight: "1px solid #ddd", overflow: "auto" }}>
        <EventList events={events} selected={selected} onSelect={setSelected} />
      </div>
      <div style={{ overflow: "auto" }}>{selected ? <EventDetail event={selected} /> : <div style={{ padding: 16 }}>No event selected</div>}</div>
    </div>
  );
}
