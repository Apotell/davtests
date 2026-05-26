module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b,c;

  sequence seq11;
    (a ##1 b) within (##3 c);
  endsequence

  assert property(@(posedge clk) seq11);

  initial begin
    #10 a=1;
    #10 b=1;
    #30 c=1;
    #60 $finish;
  end
endmodule