module dut;
  localparam logic [SIZE - 1:0] a = 0;
  localparam b = c[3][2][1:0];
  localparam d = e ? 1 : 3;
  localparam f = $sformatf("%d", g);
  localparam h = '{1, 2, 3};
  localparam bit [10:20][30:40] i = '0;
  localparam int j [10:20][30:40] = '0;
  localparam logic [10:20] k [30:40] = '0;
  
  typedef struct { logic x; } l;
  localparam l [10:20] m [30:40] = '0;
endmodule
