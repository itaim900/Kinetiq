import { describe, expect, it } from 'vitest';
import { cmToIn, formatFeetInches, inToCm, kgToLb, lbToKg, round1 } from './units';

describe('units', () => {
  it('converts length', () => {
    expect(cmToIn(2.54)).toBeCloseTo(1);
    expect(inToCm(70)).toBeCloseTo(177.8);
    expect(round1(cmToIn(178))).toBe(70.1);
  });

  it('converts weight', () => {
    expect(round1(kgToLb(74))).toBe(163.1);
    expect(lbToKg(1)).toBeCloseTo(0.45359237);
    expect(round1(lbToKg(kgToLb(74)))).toBe(74);
  });

  it('formats height as feet and inches', () => {
    expect(formatFeetInches(178)).toBe('5 ft 10 in');
    expect(formatFeetInches(182.6)).toBe('6 ft 0 in'); // 71.9 in rounds to 72 = 6 ft, not "5 ft 12 in"
  });

  it('rounds to one decimal place', () => {
    expect(round1(178.04)).toBe(178);
    expect(round1(178.05)).toBe(178.1);
  });
});
