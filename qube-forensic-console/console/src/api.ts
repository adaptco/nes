import { TelemetryEventV1 } from "./types";

export async function loadTelemetryNDJSON(url: string): Promise<TelemetryEventV1[]> {
  const res = await fetch(url);
  const text = await res.text();
  return text
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => JSON.parse(line));
}
