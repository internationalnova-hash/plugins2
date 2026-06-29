// ─────────────────────────────────────────────────────────────────────────────
// Nova Stay — Current Reservation
//
// Update these fields before each new booking.
// Guest sees their name and dates pre-filled — no entry required.
// ─────────────────────────────────────────────────────────────────────────────

const reservation = {
  guestName: "DesMar",       // Primary guest's first name (from Airbnb)
  checkIn: "July 24",        // Display format shown to guest
  checkOut: "July 26",
  nights: 2,
  bookedGuests: 8,           // Total guest count from Airbnb booking
};

export default reservation;
