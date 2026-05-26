module tb6;
  logic clk = 0, rst_n = 0;
  logic req, gnt;

  always #5 clk = ~clk;

  initial begin
    req = 0; gnt = 0;
    #8 rst_n = 1;
    #10 req = 1;
    #10 gnt = 1;
    #1000 $finish;
  end

  checker clk_chk(input logic clk, rst_n, req, gnt);

    default clocking cb @(posedge clk);
    endclocking

    default disable iff (!rst_n);

    assert property (req |-> ##1 gnt)
      else $error("Failed after reset");

  endchecker

  clk_chk chk(clk, rst_n, req, gnt);

endmodule