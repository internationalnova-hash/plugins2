import type { Metadata, Viewport } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Casanova ATL — StayByNova",
  description: "StayByNova — your AI-powered hospitality operating system, for Casanova ATL.",
  applicationName: "StayByNova",
  icons: {
    icon: "/icon.svg",
  },
};

export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  maximumScale: 1,
  themeColor: "#0A0A0A",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="bg-nova-black min-h-screen">{children}</body>
    </html>
  );
}
