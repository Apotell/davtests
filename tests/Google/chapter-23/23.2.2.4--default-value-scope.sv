/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// 23.2.2.4: the LRM's example of default port semantics and parameter scope
// resolution. "These default values shall be constant expressions evaluated
// in the scope of the module where they are defined, not in the scope of the
// instantiating module."
parameter logic [7:0] My_DataIn = 8'hFF;

module bus_conn(output logic [7:0] dataout,
                input [7:0] datain = My_DataIn);
  assign dataout = datain;
endmodule

module bus_connect1(output logic [31:0] dataout,
                    input [7:0] datain);
  parameter logic [7:0] My_DataIn = 8'h00;
  bus_conn bconn0(dataout[31:24], 8'h0F);
  bus_conn bconn1(dataout[23:16]);
  bus_conn bconn2(dataout[15:8], My_DataIn);
  bus_conn bconn3(dataout[7:0]);
endmodule
