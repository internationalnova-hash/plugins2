import AvailabilityCalendar from "@/components/AvailabilityCalendar";

export default async function BookPage({ params }: { params: { slug: string } }) {
  return (
    <main style={{ minHeight: "100vh", background: "#0a0a0a", paddingTop: 60, paddingBottom: 80 }}>
      <div style={{ textAlign: "center", marginBottom: 32 }}>
        <span style={{ color: "#b8982a", fontSize: 13, fontWeight: 600, letterSpacing: 2, textTransform: "uppercase" }}>
          StayByNova
        </span>
        <h1 style={{ color: "#fff", fontSize: 28, fontWeight: 700, margin: "8px 0 4px" }}>
          Book Your Stay
        </h1>
        <p style={{ color: "#888", fontSize: 14 }}>Direct booking — no platform fees</p>
      </div>
      <AvailabilityCalendar slug={params.slug} />
    </main>
  );
}
