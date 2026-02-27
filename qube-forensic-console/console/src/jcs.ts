export function canonicalJson(obj: any): string {
  const sortKeys = (value: any): any => {
    if (Array.isArray(value)) return value.map(sortKeys);
    if (value && typeof value === "object") {
      return Object.keys(value)
        .sort()
        .reduce((out: Record<string, any>, key) => {
          out[key] = sortKeys(value[key]);
          return out;
        }, {});
    }
    return value;
  };
  return JSON.stringify(sortKeys(obj));
}
