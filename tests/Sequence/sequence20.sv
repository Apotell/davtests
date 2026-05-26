module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq_delay_star;
    a ##[*] b;  // any delay (including 0)
  endsequence

  sequence seq_delay_plus;
    a ##[+] b;  // delay >=1
  endsequence

  assert property(@(posedge clk) seq_delay_star);
  assert property(@(posedge clk) seq_delay_plus);

  initial begin
    #10 a=1;
    #30 b=1;
    #50 $finish;
  end
endmodule