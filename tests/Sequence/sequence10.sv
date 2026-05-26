module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq10;
    a throughout (##2 b);
  endsequence

  assert property(@(posedge clk) seq10);

  initial begin
    a=1;
    #20 b=1;
    #50 $finish;
  end
endmodule