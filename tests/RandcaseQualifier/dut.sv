// IEEE 1800-2023 Sec 18.16, 37.72: a randcase is modeled as a case statement
// whose vpiQualifier is vpiRandQualifier; an ordinary case has no qualifier.

module top;
  int x, y;
  initial begin
    randcase
      1: x = 1;
      3: x = 2;
    endcase
    case (y)
      0: x = 3;
      default: x = 4;
    endcase
  end
endmodule
