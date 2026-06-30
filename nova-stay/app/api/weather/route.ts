import { NextResponse } from "next/server";
import property from "@/config/property";
import { describeWeatherCode } from "@/lib/weather";

export const revalidate = 1800; // refresh every 30 minutes

export async function GET() {
  const { lat, lon } = property.coordinates;

  try {
    const res = await fetch(
      `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current=temperature_2m,weather_code&temperature_unit=fahrenheit&timezone=auto`,
      { next: { revalidate } }
    );
    if (!res.ok) throw new Error(`Open-Meteo responded ${res.status}`);

    const data = await res.json();
    const tempF = Math.round(data.current.temperature_2m);
    const { label, icon } = describeWeatherCode(data.current.weather_code);

    return NextResponse.json({ tempF, label, icon });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Failed to fetch weather." }, { status: 502 });
  }
}
