module tb5;
  logic clk = 0;
  logic a, b;

  always #5 clk = ~clk;

  initial begin
    a = 0; b = 0;
    #10 a = 1;
    #10 b = 1;
    #1000 $finish;
  end

  checker prop_chk(input logic clk, a, b);

    property p;
      @(posedge clk) a |-> ##1 b;
    endproperty

    assert property (p) else $error("Assertion failed");
    cover property (p);

  endchecker

  prop_chk chk(clk, a, b);

endmodule