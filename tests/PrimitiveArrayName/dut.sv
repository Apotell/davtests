// IEEE 1800-2023 Sec 37.11 instance array -> str vpiName: the name of an
// array of switches (Sec 28.3.6, 28.7, 28.8) or of UDP instances (Sec 29.9).

primitive udp_and (out, a, b);
  output out;
  input a, b;
  table
    1 1 : 1;
    0 ? : 0;
    ? 0 : 0;
  endtable
endprimitive

module top;
  wire o, i, c, pc, a, b;
  nmos     mos_arr  [1:0] (o, i, c);
  cmos     cmos_arr [1:0] (o, i, c, pc);
  tran     tran_arr [1:0] (a, b);
  tranif1  trif_arr [1:0] (a, b, c);
  udp_and  udp_arr  [1:0] (o, a, b);
endmodule
