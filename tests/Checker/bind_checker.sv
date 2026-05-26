module dut(input logic clk, req, output logic gnt);

  always_ff @(posedge clk)
    gnt <= req;

endmodule


checker bind_chk(input logic clk, req, gnt);

  assert property (@(posedge clk) req |-> ##1 gnt)
    else $error("Bind failed");

endchecker


module tb8;
  logic clk = 0;
  logic req, gnt;

  always #5 clk = ~clk;

  dut d1(clk, req, gnt);

  bind dut bind_chk chk_inst(clk, req, gnt);

  initial begin
    req = 0;
    #10 req = 1;
    #100 $finish;
  end

endmodule