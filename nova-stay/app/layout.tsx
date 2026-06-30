import type { Metadata, Viewport } from "next";
import "./globals.css";

const TITLE = "Casanova ATL — StayByNova";
const DESCRIPTION = "Luxury Hospitality, Powered by Nova AI. Your personalized digital concierge for Casanova ATL.";

export const metadata: Metadata = {
  title: TITLE,
  description: DESCRIPTION,
  applicationName: "StayByNova",
  icons: {
    icon: "/icon.svg",
  },
  manifest: "/manifest.webmanifest",
  openGraph: {
    title: TITLE,
    description: DESCRIPTION,
    siteName: "StayByNova",
    type: "website",
  },
  twitter: {
    card: "summary",
    title: TITLE,
    description: DESCRIPTION,
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
