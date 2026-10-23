primitive comb_u(output o, input a, b);
  table
    0 0 : 0;
    1 1 : 1;
  endtable
endprimitive

primitive seq_u(output reg q, input d, clk);
  table
    0 r : ? : 0;
    1 r : ? : 1;
  endtable
endprimitive

primitive seq_init(output reg q, input d);
  initial q = 1'b0;
  table
    0 : ? : 0;
    1 : ? : 1;
  endtable
endprimitive
