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

// Declarations after statements in a begin or fork block (IEEE 1800-2023 Sec 9.3.1, 9.3.2).

module top;
  int a;
  initial begin
    a = 1;
    int b;
    b = 2;
  end
  initial fork : f
    a = 1;
    int z;
  join
  initial begin
    int x = 1;
    int y;
    y = x;
  end
endmodule
