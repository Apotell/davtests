primitive u1(output o, input a, b);
  table
    0 0 : 0;
    1 1 : 1;
  endtable
endprimitive

module top(input a, b);
  wire o1, o2, o3, o4, o5;
  and (strong0, weak1) g_sw(o1, a, b);
  or (weak1, pull0) g_pw(o2, a, b);
  nand (highz1, strong0) g_sz(o3, a, b);
  pullup (pull1) p_up(o4);
  pulldown (strong0) p_dn(o4);
  u1 (supply0, pull1) u_sp(o5, a, b);
endmodule
