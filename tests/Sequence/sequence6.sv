module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq6;
    a and b;
  endsequence

  assert property(@(posedge clk) seq6);

  initial begin
    #10 a=1; b=1;
    #30 $finish;
  end
endmodule