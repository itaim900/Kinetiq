// Metric <-> imperial conversions. The server always stores metric; the UI
// converts for display and converts back before sending.
export type Units = 'metric' | 'imperial';

export const CM_PER_IN = 2.54; // exact, by definition
export const KG_PER_LB = 0.45359237; // exact, by definition

// One decimal place: what we show and what we send.
export const round1 = (n: number) => Math.round(n * 10) / 10;

export const cmToIn = (cm: number) => cm / CM_PER_IN;
export const inToCm = (inches: number) => inches * CM_PER_IN;
export const kgToLb = (kg: number) => kg / KG_PER_LB;
export const lbToKg = (lb: number) => lb * KG_PER_LB;

// "5 ft 10 in" -- rounds total inches first, so it never shows "5 ft 12 in".
export function formatFeetInches(cm: number): string {
  const totalInches = Math.round(cmToIn(cm));
  return `${Math.floor(totalInches / 12)} ft ${totalInches % 12} in`;
}
