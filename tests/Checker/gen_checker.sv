module tb3;
  logic clk = 0;
  logic [3:0] a, b;

  always #5 clk = ~clk;

  initial begin
    a = 4'b0000; b = 4'b0000;
    #10 a = 4'b1111;
    #10 b = 4'b1111;
    #1000 $finish;
  end

  checker gen_chk(input logic clk,
                  input logic [3:0] a, b);

    genvar i;
    generate
      for (i = 0; i < 4; i++) begin
        assert property (@(posedge clk) a[i] |-> ##1 b[i])
          else $error("Bit %0d failed", i);
      end
    endgenerate

  endchecker

  gen_chk chk(clk, a, b);

endmodule