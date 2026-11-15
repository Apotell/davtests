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

// 23.2.2.2, Syntax 23-4: a variable port may carry "= constant_expression",
// and footnote 2 forbids it only for "a port that is not a variable output
// port". x is a variable output port, so its initializer is legal.
module m(output var logic x = 1'b1);
endmodule
