// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Active Bookings
//
// Add one entry per upcoming reservation. The confirmation code is whatever
// Airbnb gives the guest — they type it in during check-in and the app
// matches it here to personalize their guide. Safe to have many active
// bookings at once (e.g. overlapping weekends).
//
// Remove an entry once that stay is over — it's just for matching, not history.
// ─────────────────────────────────────────────────────────────────────────────

export interface Booking {
  confirmationCode: string; // from Airbnb, case-insensitive
  guestName: string;
  checkIn: string;          // display format shown to guest
  checkOut: string;
  nights: number;
  bookedGuests: number;     // total guest count from Airbnb booking
}

const bookings: Booking[] = [
  {
    confirmationCode: "HMDESMAR24",
    guestName: "DesMar",
    checkIn: "July 24",
    checkOut: "July 26",
    nights: 2,
    bookedGuests: 8,
  },
  {
    confirmationCode: "HMKAMEDA01",
    guestName: "Kameda",
    checkIn: "July 4",
    checkOut: "July 6",
    nights: 2,
    bookedGuests: 6,
  },
];

export default bookings;
