module top(input clk, rst, d, output q, r);
  specify
    (posedge clk => (q +: d)) = 1;
    (negedge rst *> (q, r : d)) = 2;
    (edge clk => (r -: d)) = 3;
    (clk => (q : d)) = 4;
    (d => q) = 5;
  endspecify
endmodule
