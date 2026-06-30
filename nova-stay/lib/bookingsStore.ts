// ─────────────────────────────────────────────────────────────────────────────
// Nova Stay — Bookings Store
//
// Reservations are created and managed through the Host Reservation Manager
// UI (inside Host View), not by editing source files. This module is the
// single read/write boundary for that data — currently backed by
// localStorage, seeded once from config/bookings.ts as starter data.
//
// Swapping this for a real database later means rewriting the functions
// below, not the UI or onboarding flow that call them.
// ─────────────────────────────────────────────────────────────────────────────

import seedBookings, { type Booking } from "@/config/bookings";

const KEY = "novaStay_bookings";

function readRaw(): Booking[] {
  if (typeof window === "undefined") return seedBookings;
  try {
    const raw = localStorage.getItem(KEY);
    if (raw) return JSON.parse(raw) as Booking[];
  } catch { /* ignore */ }
  localStorage.setItem(KEY, JSON.stringify(seedBookings));
  return seedBookings;
}

function writeRaw(bookings: Booking[]) {
  localStorage.setItem(KEY, JSON.stringify(bookings));
}

export function getBookings(): Booking[] {
  return readRaw();
}

export function findBooking(code: string): Booking | undefined {
  const target = code.trim().toUpperCase();
  return readRaw().find((b) => b.confirmationCode.toUpperCase() === target);
}

export function addBooking(booking: Booking): Booking[] {
  const updated = [...readRaw(), booking];
  writeRaw(updated);
  return updated;
}

export function deleteBooking(confirmationCode: string): Booking[] {
  const updated = readRaw().filter((b) => b.confirmationCode !== confirmationCode);
  writeRaw(updated);
  return updated;
}

export type { Booking };
