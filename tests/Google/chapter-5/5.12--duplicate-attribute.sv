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

// The attribute keep is given twice on one net (IEEE 1800-2023 Sec 5.12: the last value is used).

module top;
  (* keep = 1, keep = 0 *) wire w;
  (* keep = 1, dont_touch = 1 *) wire ok;
endmodule
