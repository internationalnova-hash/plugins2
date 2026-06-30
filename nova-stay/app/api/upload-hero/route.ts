import { NextRequest, NextResponse } from "next/server";
import { put } from "@vercel/blob";
import { isAuthorizedHost } from "@/lib/hostAuth";

const MAX_BYTES = 8 * 1024 * 1024;
const ALLOWED_TYPES = ["image/jpeg", "image/png", "image/webp", "image/avif"];

export async function POST(req: NextRequest) {
  if (!(await isAuthorizedHost(req))) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const form = await req.formData();
  const file = form.get("file");
  if (!(file instanceof File)) {
    return NextResponse.json({ error: "No file uploaded." }, { status: 400 });
  }
  if (!ALLOWED_TYPES.includes(file.type)) {
    return NextResponse.json({ error: "Please upload a JPEG, PNG, WEBP, or AVIF image." }, { status: 400 });
  }
  if (file.size > MAX_BYTES) {
    return NextResponse.json({ error: "Image is too large (8MB max)." }, { status: 400 });
  }

  try {
    const ext = file.type.split("/")[1];
    const blob = await put(`hero/${Date.now()}.${ext}`, file, {
      access: "public",
      addRandomSuffix: true,
    });
    return NextResponse.json({ url: blob.url });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Upload failed." }, { status: 500 });
  }
}
