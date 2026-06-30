import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldBright;

export default function SNMonogram({ size = 40, id = "sn" }: { size?: number; id?: string }) {
  const gradId = `sn-grad-${id}`;
  return (
    <svg width={size} height={size} viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id={gradId} x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor={GOLD} />
          <stop offset="50%" stopColor={GOLD_LIGHT} />
          <stop offset="100%" stopColor={GOLD} />
        </linearGradient>
      </defs>
      {/* S */}
      <path
        d="M30 10C30 10 23 8 19 11C15 14 16 18 20 20C24 22 28 22 28 27C28 32 22 33 18 31"
        stroke={`url(#${gradId})`}
        strokeWidth="3.4"
        strokeLinecap="round"
        fill="none"
      />
      {/* N */}
      <path
        d="M14 38V16M14 16L34 38M34 38V16"
        stroke={`url(#${gradId})`}
        strokeWidth="3.4"
        strokeLinecap="round"
        strokeLinejoin="round"
        fill="none"
      />
    </svg>
  );
}
