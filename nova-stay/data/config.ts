export const propertyConfig = {
  brand: {
    name: "StayByNova",
    property: "Casanova ATL",
    tagline: "Your luxury home away from home.",
    guideUrl: "https://nova-stay.vercel.app",
  },

  welcome: {
    heading: "Welcome Home.",
    subheading: "A StayByNova Experience",
    message:
      "Thank you for choosing Casanova ATL. This guide contains everything you need for an unforgettable luxury stay.",
    hostName: "Cristina N.",
  },

  gameRoom: {
    heading: "Game Room",
    details: [
      "Available 24 hours for all guests.",
      "Please keep the volume at a respectful level after 10:00 PM.",
      "Return all controllers and equipment to their original positions.",
      "Do not bring food or drinks near the gaming equipment.",
    ],
  },

  wifi: {
    networks: [
      { name: "EMG", password: "Takitaki123#" },
      { name: "The Society", password: "Takitaki123#" },
    ],
  },

  doorCode: {
    code: "1710#",
    note: "Enter the code on the keypad, then press the # key to unlock.",
  },

  theater: {
    heading: "Movie Theater",
    steps: [
      "Flip the light switches inside the theater room — this powers on the projector.",
      "Press the power button on the projector itself to activate the display.",
      "Use the Fire Stick remote from the theater control holder to navigate apps.",
      "Surround sound is wireless — control it from the tablet in the room.",
      "Tablet passcode: 2121",
    ],
  },

  studio: {
    heading: "Recording Studio Add-On",
    description:
      "Take your stay to the next level with access to our professional recording studio. Book a session and create something unforgettable.",
    packages: [
      { duration: "4-Hour Session", price: "$150", includes: "Studio access" },
      {
        duration: "8-Hour Session",
        price: "$300",
        includes: "Studio access + Engineer included",
      },
    ],
    bookingNote:
      "Contact your host to add a studio session to your stay.",
  },

  pool: {
    heading: "Pool & Backyard",
    rules: [
      "No glass near the pool area.",
      "Children must be supervised at all times.",
      "Please shower before entering the pool.",
      "No jumping or diving — shallow areas present.",
      "Keep music at a respectful volume outdoors.",
      "Outdoor quiet hours begin at 8:00 PM — no loud music or gatherings outside after this time.",
    ],
  },

  quietHours: {
    outdoorCutoff: "8:00 PM",
    note: "Outdoor noise, music, and gatherings must end by 8:00 PM to respect our neighbors. Indoor enjoyment can continue at a reasonable volume.",
  },

  driveway: {
    heading: "Driveway & Front of Home",
    note: "Please be mindful of the driveway and front of the home. Avoid congregating in the driveway or on the street, as this can disturb neighbors and attract unwanted attention.",
  },

  grill: {
    heading: "Grill & Outdoor Lights",
    instructions: [
      "Outside lights are controlled by the white oval control panel near the back door.",
      "Grill is available for guest use — please clean it after each use.",
      "Never leave the grill unattended while in use.",
      "Allow the grill to fully cool before covering.",
    ],
  },

  checkout: {
    heading: "Checkout Instructions",
    time: "11:00 AM",
    steps: [
      "Strip all bed linens and leave them in a pile on the floor of each bedroom.",
      "Place all used towels in the bathtub or laundry basket.",
      "Wash and put away any dishes you used.",
      "Dispose of all trash in the outdoor bins.",
      "Turn off all lights, TVs, and the AC before leaving.",
      "Lock the front door using the keypad.",
      "Leave keys (if any) on the kitchen counter.",
      "Share your experience — a 5-star review means the world to us.",
    ],
  },

  emergency: {
    contacts: [
      { label: "Host — Cristina N.", value: "Contact via Airbnb app" },
      { label: "Police / Fire / Medical", value: "911" },
      { label: "Poison Control", value: "1-800-222-1222" },
      { label: "Nearest Hospital", value: "Grady Memorial Hospital — 0.8 mi" },
    ],
  },

  recommendations: [
    {
      category: "Dining",
      places: [
        { name: "STK Atlanta", note: "Upscale steakhouse, Midtown" },
        { name: "Slutty Vegan", note: "Iconic plant-based burgers" },
        { name: "The Optimist", note: "Fresh seafood, West Midtown" },
      ],
    },
    {
      category: "Nightlife",
      places: [
        { name: "Gold Room", note: "Upscale nightclub" },
        { name: "Believe Music Hall", note: "Live events & concerts" },
        { name: "SL Lounge", note: "Rooftop bar, Buckhead" },
      ],
    },
    {
      category: "Things To Do",
      places: [
        { name: "Ponce City Market", note: "Shopping, food, rooftop" },
        { name: "BeltLine Eastside Trail", note: "Walking, art, vibes" },
        { name: "Trap Music Museum", note: "Atlanta culture" },
      ],
    },
  ],

  // ── Explore tab — curated, not exhaustive ──────────────────────────────
  explore: [
    {
      category: "Attractions",
      places: [
        { name: "Georgia Aquarium", note: "World's largest indoor aquarium" },
        { name: "World of Coca-Cola", note: "Iconic Atlanta experience" },
        { name: "Trap Music Museum", note: "Atlanta culture, downtown" },
      ],
    },
    {
      category: "Braves Stadium",
      places: [
        { name: "Truist Park", note: "Home of the Atlanta Braves — ~25 min" },
        { name: "The Battery Atlanta", note: "Dining & entertainment district at the stadium" },
      ],
    },
    {
      category: "Downtown Atlanta",
      places: [
        { name: "Centennial Olympic Park", note: "Heart of downtown, fountains & events" },
        { name: "Ponce City Market", note: "Shopping, food hall, rooftop views" },
      ],
    },
    {
      category: "Trilith Studios",
      places: [
        { name: "Trilith Studios", note: "Major film production studio — ~30 min" },
        { name: "Town at Trilith", note: "Walkable shops & restaurants nearby" },
      ],
    },
    {
      category: "Shopping",
      places: [
        { name: "Lenox Square", note: "Premier shopping mall, Buckhead" },
        { name: "Phipps Plaza", note: "Luxury retail, Buckhead" },
      ],
    },
    {
      category: "Parks",
      places: [
        { name: "BeltLine Eastside Trail", note: "Walking, art, local vibes" },
        { name: "Piedmont Park", note: "Atlanta's flagship green space" },
      ],
    },
  ],

  // ── Dining tab — curated picks per meal, not a directory ───────────────
  dining: [
    {
      category: "Breakfast",
      places: [
        { name: "Flying Biscuit Cafe", note: "Southern comfort breakfast" },
        { name: "Brek Coffee House", note: "Light bites, great espresso" },
      ],
    },
    {
      category: "Lunch",
      places: [
        { name: "Bone Lick BBQ", note: "Atlanta barbecue staple" },
        { name: "Slutty Vegan", note: "Iconic plant-based burgers" },
      ],
    },
    {
      category: "Dinner",
      places: [
        { name: "The Optimist", note: "Fresh seafood, West Midtown" },
        { name: "STK Atlanta", note: "Upscale steakhouse, Midtown" },
      ],
    },
    {
      category: "Fine Dining",
      places: [
        { name: "Bazati", note: "Modern fine dining, Buckhead" },
        { name: "Aria", note: "Elevated New American, Buckhead" },
      ],
    },
    {
      category: "Delivery",
      places: [
        { name: "DoorDash", note: "Available property-wide" },
        { name: "Uber Eats", note: "Available property-wide" },
      ],
    },
    {
      category: "Coffee",
      places: [
        { name: "Chrome Yellow Trading Co.", note: "Local roaster, Westside" },
        { name: "Brek Coffee House", note: "Cozy, walkable" },
      ],
    },
    {
      category: "Nightlife",
      places: [
        { name: "Gold Room", note: "Upscale nightclub" },
        { name: "SL Lounge", note: "Rooftop bar, Buckhead" },
      ],
    },
  ],

  faq: [
    {
      q: "Is early check-in available?",
      a: "Early check-in may be available depending on the schedule. Message your host on Airbnb to request it.",
    },
    {
      q: "Is late checkout available?",
      a: "Late checkout is occasionally available. Please request via Airbnb at least 24 hours in advance.",
    },
    {
      q: "Are pets allowed?",
      a: "Pets are not permitted at Casanova ATL unless explicitly approved by the host.",
    },
    {
      q: "How many guests are allowed?",
      a: "Please refer to your Airbnb booking for the approved guest count. Unauthorized guests are not permitted.",
    },
    {
      q: "Is there parking?",
      a: "Yes — driveway parking is available. Please be courteous and avoid blocking the street or neighbors' driveways.",
    },
    {
      q: "What if something is broken or not working?",
      a: "Please contact your host immediately via the Airbnb app so we can resolve it quickly.",
    },
  ],
};
