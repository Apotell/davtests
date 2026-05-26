module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b,c;

  sequence seq2;
    a ##1 b ##2 c;
  endsequence

  assert property(@(posedge clk) seq2);

  initial begin
    a=0;b=0;c=0;
    #10 a=1;
    #10 b=1;
    #20 c=1;
    #50 $finish;
  end
endmodule