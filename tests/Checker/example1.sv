module tb1;
  logic clk = 0;
  logic req, gnt;

  always #5 clk = ~clk;

  // DUT stimulus
  initial begin
    $display("hello exmp1");
    req = 0; gnt = 0;
    #10 req = 1;
    #10 gnt = 1;
    #10 req = 0; gnt = 0;
    #1000 $finish;
  end

  // Checker
  checker basic_chk(input logic clk, req, gnt);
    assert property (@(posedge clk) req |-> ##1 gnt)
      else $error("Grant not received!");
  endchecker

  basic_chk chk1(clk, req, gnt);

endmodule