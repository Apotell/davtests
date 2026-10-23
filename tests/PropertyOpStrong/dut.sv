// IEEE 1800-2023 Sec 37.52 detail 3: vpiOpStrong is TRUE for the strong form
// of an until operator (Sec 16.12.13: s_until, s_until_with).

module top(input logic clk, a, b);
  property p_until; a until b; endproperty
  property p_s_until; a s_until b; endproperty
  property p_until_with; a until_with b; endproperty
  property p_s_until_with; a s_until_with b; endproperty
endmodule
