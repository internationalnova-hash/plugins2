// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Current Reservation
//
// Update these fields before each new booking.
// Guest sees their name and dates pre-filled — no entry required.
// ─────────────────────────────────────────────────────────────────────────────

const reservation = {
  guestName: "Jamilah",      // Primary guest's first name (from Airbnb)
  checkIn: "July 18",        // Display format shown to guest
  checkOut: "July 20",
  nights: 2,
  bookedGuests: 8,           // Total guest count from Airbnb booking
};

export default reservation;
