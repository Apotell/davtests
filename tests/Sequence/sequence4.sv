module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence base_seq;
    a;
  endsequence

  sequence seq4;
    base_seq[*3]; // repetition on sequence instance
  endsequence

  assert property(@(posedge clk) seq4);

  initial begin
    a=0;
    repeat(3) #10 a=1;
    #50 $finish;
  end
endmodule