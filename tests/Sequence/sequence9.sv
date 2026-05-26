module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq9;
    first_match(a ##[1:3] b);
  endsequence

  assert property(@(posedge clk) seq9);

  initial begin
    #10 a=1;
    #20 b=1;
    #50 $finish;
  end
endmodule