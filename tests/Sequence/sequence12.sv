module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq12;
    @(posedge clk) a;
  endsequence

  assert property(seq12);

  initial begin
    #10 a=1;
    #30 $finish;
  end
endmodule