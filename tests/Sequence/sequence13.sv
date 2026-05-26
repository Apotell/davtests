module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq13;
    a ##[1:3] b; // range delay
  endsequence

  assert property(@(posedge clk) seq13);

  initial begin
    #10 a=1;
    #20 b=1;
    #50 $finish;
  end
endmodule