module tb4;
  logic clk = 0;
  logic x, y;

  always #5 clk = ~clk;

  initial begin
    x = 0; y = 0;
    #10 x = 1;
    #10 y = 1;
    #1000 $finish;
  end

  checker outer_chk(input logic clk, x, y);

    checker inner_chk(input logic a, b);
      assert property (@(posedge clk) a |-> ##1 b)
        else $error("Inner failed");
    endchecker

    inner_chk ic(x, y);

  endchecker

  outer_chk oc(clk, x, y);

endmodule