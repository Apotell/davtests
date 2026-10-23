// IEEE 1800-2023 Sec 37.51 detail 5 / 37.53 detail 4: vpiDirection of a
// property/sequence formal is vpiNoDirection unless it is a local variable
// formal argument (Sec 16.8.2, 16.12.19), whose direction defaults to input.
// Sec 37.51 detail 4: a formal's default is reachable through vpiExpr.

module top(input logic clk);
  property p(a, local input int b = 1, local int c = 2, sequence s = a);
    @(posedge clk) a;
  endproperty

  sequence q(x, local input int y, local inout int z, local output int w, local int v);
    x;
  endsequence

  let L(lx, int ly = 1) = lx + ly;

  sequence sq; clk ##1 clk; endsequence

  property pd(sequence s1 = @(posedge clk) clk ##1 clk,
              property r2 = clk |-> clk,
              sequence s2 = sq,
              untyped u = clk);
    s1;
  endproperty
endmodule
