// Maps Open-Meteo's WMO weather codes to a label + Lucide icon name.
// https://open-meteo.com/en/docs#weathervariables

export interface WeatherInfo {
  tempF: number;
  label: string;
  icon: "Sun" | "CloudSun" | "Cloud" | "CloudFog" | "CloudDrizzle" | "CloudRain" | "CloudSnow" | "CloudLightning";
}

export function describeWeatherCode(code: number): { label: string; icon: WeatherInfo["icon"] } {
  if (code === 0) return { label: "Clear skies", icon: "Sun" };
  if (code === 1 || code === 2) return { label: "Partly cloudy", icon: "CloudSun" };
  if (code === 3) return { label: "Overcast", icon: "Cloud" };
  if (code === 45 || code === 48) return { label: "Foggy", icon: "CloudFog" };
  if (code >= 51 && code <= 57) return { label: "Drizzle", icon: "CloudDrizzle" };
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return { label: "Rain showers", icon: "CloudRain" };
  if ((code >= 71 && code <= 77) || code === 85 || code === 86) return { label: "Snow", icon: "CloudSnow" };
  if (code >= 95) return { label: "Thunderstorms", icon: "CloudLightning" };
  return { label: "Clear skies", icon: "Sun" };
}
