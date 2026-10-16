interface gen_if_case #(parameter int MODE = 1, parameter int N = 2) (input logic clk);
  logic [7:0] data;
  logic       valid;
  logic       ready;

  generate
    if (MODE == 1)
      assign ready = valid;
    else
      assign ready = 1'b0;

    if (MODE == 2) g_m2: begin
      logic x;
      assign x = valid;
    end

    if (N == 1)
      assign data = 8'h01;
    else if (N == 2)
      assign data = 8'h02;
    else
      assign data = 8'h00;

    case (N)
      1: assign valid = 1'b0;
      2: g_two: begin
        logic [1:0] sel;
        assign sel = 2'b01;
      end
      default: assign valid = 1'b1;
    endcase
  endgenerate
endinterface
