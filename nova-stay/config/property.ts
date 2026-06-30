// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Property Configuration
//
// To deploy this guide for a new property:
//   1. Drop the hero photo into /public/images/hero/
//   2. Update the fields below
//   3. Redeploy to Vercel
//
// No component code needs to change.
// ─────────────────────────────────────────────────────────────────────────────

const property = {
  // Identity
  name: "Casanova ATL",
  city: "Atlanta, Georgia",
  tagline: "Your Luxury Home Away From Home",
  amenities: ["Luxury Pool", "Theater", "Recording Studio", "Game Room"],

  // Hero image — drop file into /public/images/hero/ and update path here
  heroImage: "/images/hero/casanova-hero.jpg",

  // Social links (set to "" to hide)
  social: {
    instagram: "https://instagram.com/internationalnova",
    facebook: "",
    youtube: "",
    tiktok: "",
  },
};

export default property;
