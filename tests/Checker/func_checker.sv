module tb2;
  logic clk = 0;
  logic [3:0] data;

  always #5 clk = ~clk;

  initial begin
    data = 4'b0000;
    #10 data = 4'b1111;
    #1000 $finish;
  end

  checker func_chk(input logic clk, input logic [3:0] data);

    function int ones(input logic [3:0] d);
      return $countones(d);
    endfunction

    assert property (@(posedge clk) ones(data) <= 4)
      else $error("Too many ones!");

  endchecker

  func_chk chk(clk, data);

endmodule