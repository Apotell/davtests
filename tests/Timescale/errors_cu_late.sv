// IEEE 1800-2023 Sec 3.14.2.2: "There shall be at most one time unit and one time
// precision for any ... compilation-unit scope ... If specified, the timeunit and
// timeprecision declarations shall precede any other items in the current time scope."
// Here the compilation-unit timeunit comes after a module.

module e_cu_before;
endmodule

timeunit 1ns;

module e_cu_after;
endmodule
