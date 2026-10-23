// IEEE 1800-2023 Sec 37.16 vpiStrength0 / vpiStrength1 / vpiChargeStrength of a net:
// the drive strength (Sec 6.3.2.2) or charge strength (Sec 6.3.2.1) of its declaration.

module top;
  wire (strong0, weak1) drv_sw;
  wire (pull1, highz0) drv_ph;
  wire (supply0, supply1) drv_ss;
  wire plain;
  trireg (small) chg_small;
  trireg (large) chg_large;
  trireg chg_default;
endmodule
