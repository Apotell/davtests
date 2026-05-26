module tb7;
  logic clk = 0;
  logic a, b;

  always #5 clk = ~clk;

  initial begin
    a = 1; b = 0;
    #10 b = 1;
    #1000 $finish;
  end

  checker init_final_chk(input logic clk, a, b);

    initial $display("Checker started");

    final $display("Checker finished");

    assert property (@(posedge clk) a |-> ##1 b);

  endchecker

  init_final_chk chk(clk, a, b);

endmodule