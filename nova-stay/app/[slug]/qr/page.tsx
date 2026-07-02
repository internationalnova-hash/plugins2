"use client";

import { useParams } from "next/navigation";
import { QRCodeSVG } from "qrcode.react";
import { useEffect, useState } from "react";

export default function QRPrintPage() {
  const params = useParams<{ slug: string }>();
  const slug =
    typeof params?.slug === "string"
      ? params.slug
      : Array.isArray(params?.slug)
      ? params.slug[0]
      : "";

  const [origin, setOrigin] = useState("");
  useEffect(() => {
    setOrigin(window.location.origin);
  }, []);

  const url = origin && slug ? `${origin}/${slug}` : "";

  return (
    <div className="qr-page">
      <style>{`
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { background: #fff; }
        .qr-page {
          min-height: 100vh;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          background: #fff;
          font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
          gap: 32px;
          padding: 48px 24px;
        }
        .card {
          display: flex;
          flex-direction: column;
          align-items: center;
          gap: 28px;
          border: 2px solid #e5e5e5;
          border-radius: 24px;
          padding: 48px 56px;
          max-width: 480px;
          width: 100%;
        }
        .logo {
          font-size: 22px;
          font-weight: 700;
          letter-spacing: 0.04em;
          color: #111;
        }
        .logo span {
          color: #B59A55;
        }
        .qr-wrap {
          background: #fff;
          border-radius: 16px;
          padding: 12px;
          box-shadow: 0 0 0 2px #e5e5e5;
        }
        .instructions {
          text-align: center;
          display: flex;
          flex-direction: column;
          gap: 6px;
        }
        .instructions p {
          font-size: 18px;
          color: #111;
          font-weight: 600;
        }
        .instructions small {
          font-size: 13px;
          color: #888;
          word-break: break-all;
        }
        .print-btn {
          position: fixed;
          bottom: 32px;
          right: 32px;
          background: #111;
          color: #fff;
          border: none;
          border-radius: 12px;
          padding: 14px 28px;
          font-size: 16px;
          font-weight: 600;
          cursor: pointer;
          display: flex;
          align-items: center;
          gap: 8px;
          box-shadow: 0 4px 16px rgba(0,0,0,0.2);
        }
        @media print {
          .print-btn { display: none !important; }
          .qr-page { justify-content: center; min-height: 100vh; }
        }
      `}</style>

      <div className="card">
        <div className="logo">
          Stay by <span>Nova</span>
        </div>

        <div className="qr-wrap">
          {url ? (
            <QRCodeSVG
              value={url}
              size={240}
              bgColor="#ffffff"
              fgColor="#111111"
              level="M"
            />
          ) : (
            <div style={{ width: 240, height: 240, background: "#f5f5f5", borderRadius: 8 }} />
          )}
        </div>

        <div className="instructions">
          <p>Scan for your digital guide</p>
          {url && <small>{url}</small>}
        </div>
      </div>

      <button className="print-btn" onClick={() => window.print()}>
        🖨️ Print
      </button>
    </div>
  );
}
