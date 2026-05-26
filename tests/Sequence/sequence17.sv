module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq_nonconsec;
    a[=2];  // exactly 2 matches, gaps allowed
  endsequence

  assert property(@(posedge clk) seq_nonconsec);

  initial begin
    a=0;
    #10 a=1;
    #10 a=0;
    #10 a=1;
    #40 $finish;
  end
endmodule