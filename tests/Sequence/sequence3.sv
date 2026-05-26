module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq3;
    a[*2]; // consecutive repetition
  endsequence

  assert property(@(posedge clk) seq3);

  initial begin
    a=0;
    #10 a=1;
    #10 a=1;
    #20 $finish;
  end
endmodule