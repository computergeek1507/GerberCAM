#pragma once
/*
 * Internal unit system: 1 mm = PRECISIONSCALE (1,000,000) internal units.
 * → 1 internal unit = 1 nanometre (nm).
 * Gerber files are in inches; the parser multiplies by PRECISIONSCALE * 25.4
 * to convert to internal mm-based units.
 *
 * PRECISION controls the decimal-digit padding inside convertNumber().
 * It must remain 6 so that raw gerber integers align with a 1-microinch
 * step before the ×25.4 conversion is applied.
 */

static double PRECISION = 6.0;
static double PRECISIONSCALE = 1000000.0;  // internal units per mm
static double PRECISIONERROR = 25400.0;    // ~0.0254 mm (≈1 mil) tolerance