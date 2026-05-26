module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq8;
    a or b;
  endsequence

  assert property(@(posedge clk) seq8);

  initial begin
    #10 a=1;
    #20 b=1;
    #40 $finish;
  end
endmodule