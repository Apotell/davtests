module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq_range;
    a[*2:4];  // between 2 and 4 consecutive matches
  endsequence

  assert property(@(posedge clk) seq_range);

  initial begin
    #10 a=1;
    #10 a=1;
    #10 a=1;
    #40 $finish;
  end
endmodule