module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq7;
    a intersect b;
  endsequence

  assert property(@(posedge clk) seq7);

  initial begin
    #10 a=1; b=1;
    #30 $finish;
  end
endmodule