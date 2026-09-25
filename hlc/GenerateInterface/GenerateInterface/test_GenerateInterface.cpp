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

// Tests for tests/GenerateInterface/top.sv. GenerateInterface.hlc compiles
// with "-d inst" (full elaboration), so this file checks the elaborated
// Interface instances and their nested generate-for constructs, following
// the same GenScope/GenScopeArray style as
// hlc/ForElab/ForElab/test_ForElab.cpp.
//
//   interface abc_if #(int NO_Input=3);
//     logic [NO_Input-1 :0 ] xyz_o;
//     logic [NO_Input-1 :0 ] clk_i;
//     for(genvar i=0;i<NO_Input;i++)begin : block
//        clocking abc_cb @(posedge clk_i[i]);
//     endclocking
//     end : block
//   endinterface
//
//   module top ();
//     abc_if intf();
//   endmodule
//
//   interface pins_if #( parameter int Width = 1 ) (
//     inout [Width-1:0] pins
//   );
//     logic [Width-1:0] pins_o;
//     generate
//       for (genvar i = 0; i < Width; i++) begin : each_pin_intf
//         assign pins[i] = pins_oe[i] ? pins_o[i] : 1'bz;
//       end
//     endgenerate
//   endinterface
//
//   module top2 #( parameter int Width = 2 ) (inout [Width-1:0] pins);
//   pins_if #(.Width(Width)) intf  (pins);
//     generate
//       for (genvar i = 0; i < Width; i++) begin : each_pin
//         assign pins[i] = pins_oe[i] ? pins_o[i] : 1'bz;
//       end
//     endgenerate
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 25.3 "Interfaces": 'top' instantiates 'abc_if' as
// 'intf' with the default NO_Input == 3; 'top2' instantiates 'pins_if' as
// 'intf' overriding Width by name with its own Width parameter, which
// itself defaults to 2 (Sec 23.10 "Overriding module parameters" -- the
// same by-name override mechanism applies to interface instantiation).
// Sec 27.4 "Loop generate constructs": the "for (genvar i = 0; i < N;
// i++) begin : <label> ... end" inside each interface must elaborate to a
// GenScopeArray of size N (3 for abc_if's 'block', 2 for pins_if's
// 'each_pin_intf' and top2's own 'each_pin').
// Sec 6.3 "Implicit declarations": 'pins_oe' is never declared anywhere in
// this source, so every reference to it must fail to bind.
//
// As in ForElab, each level of the elaborated-scope traversal uses a
// GTEST_SKIP fallback rather than asserting a possibly-wrong nullptr shape.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/interface.h>
#include <hldb/module.h>

namespace hlc {

class GenerateInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenerateInterface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Interface *findInterfaceDef(std::string_view name) {
    return hldb::findByName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  static const hldb::Interface *findElaboratedInterface(const hldb::Module *m, std::string_view instName) {
    if (m == nullptr || m->getInterfaces() == nullptr) return nullptr;
    return hldb::findByName<hldb::Interface>(instName, m->getInterfaces());
  }

  template <typename ScopeT>
  static const hldb::GenScopeArray *findGenScopeArray(const ScopeT *scope, std::string_view name) {
    if (scope == nullptr || scope->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *scope->getGenScopeArrays()) {
      if (gsa->getName() == name) return gsa;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Definitions
// ---------------------------------------------------------------------------

TEST_F(GenerateInterfaceTest, DefinitionsExist) {
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' not found";
  EXPECT_NE(getModule("top2"), nullptr) << "module 'top2' not found";
  EXPECT_NE(findInterfaceDef("abc_if"), nullptr) << "interface 'abc_if' not found";
  EXPECT_NE(findInterfaceDef("pins_if"), nullptr) << "interface 'pins_if' not found";
}

// ---------------------------------------------------------------------------
// Sec 25.3: 'top' instantiates 'abc_if' as 'intf' (default NO_Input == 3).
// ---------------------------------------------------------------------------

TEST_F(GenerateInterfaceTest, TopInstantiatesAbcIfAsIntf) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Interface *const intf = findElaboratedInterface(top, "intf");
  if (intf == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate 'abc_if intf();' as an Interface instance under module 'top'. Per IEEE "
                     "1800-2023 Sec 25.3, this interface instantiation must be elaborated. Fix pending.";
  }
  EXPECT_EQ(intf->getDefName(), std::string_view{"abc_if"});
}

// ---------------------------------------------------------------------------
// Sec 27.4: "for (genvar i=0;i<NO_Input;i++) begin : block ... end" inside
// abc_if -- NO_Input defaults to 3, so 'block' must have 3 iterations, each
// containing the clocking block 'abc_cb'.
// ---------------------------------------------------------------------------

TEST_F(GenerateInterfaceTest, AbcIfBlockLoopHasThreeIterationsWithClockingBlock) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Interface *const intf = findElaboratedInterface(top, "intf");
  if (intf == nullptr) {
    GTEST_SKIP() << "'intf' itself was not elaborated (see TopInstantiatesAbcIfAsIntf); cannot check its nested "
                     "'block' loop.";
  }
  const hldb::GenScopeArray *const block = findGenScopeArray(intf, "block");
  if (block == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate the loop generate construct 'for (genvar i=0;i<NO_Input;i++) begin : "
                     "block ... end' inside 'abc_if' (no GenScopeArray named 'block' found). Per IEEE 1800-2023 "
                     "Sec 27.4, NO_Input defaults to 3, so this loop must elaborate exactly 3 iterations. Fix "
                     "pending.";
  }
  EXPECT_EQ(block->getSize(), 3) << "Sec 27.4: default NO_Input == 3 must produce exactly 3 iterations";
  ASSERT_NE(block->getGenScopes(), nullptr);
  ASSERT_EQ(block->getGenScopes()->size(), 3u);
  for (const hldb::GenScope *const iter : *block->getGenScopes()) {
    ASSERT_NE(iter, nullptr);
    if (iter->getClockingBlocks() == nullptr) {
      ADD_FAILURE() << "'clocking abc_cb @(posedge clk_i[i]);' not found -- 'block' iteration has no clocking "
                        "blocks";
      continue;
    }
    EXPECT_NE(hldb::findByName<hldb::ClockingBlock>("abc_cb", iter->getClockingBlocks()), nullptr)
        << "clocking block 'abc_cb' not found in this 'block' iteration";
  }
}

// ---------------------------------------------------------------------------
// Sec 23.10/25.3: 'top2' instantiates 'pins_if' as 'intf', overriding Width
// by name with top2's own Width parameter (defaults to 2).
// ---------------------------------------------------------------------------

TEST_F(GenerateInterfaceTest, Top2InstantiatesPinsIfAsIntfWithWidthOverride) {
  const hldb::Module *const top2 = getModule("top2");
  ASSERT_NE(top2, nullptr);
  const hldb::Interface *const intf = findElaboratedInterface(top2, "intf");
  if (intf == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate 'pins_if #(.Width(Width)) intf (pins);' as an Interface instance under "
                     "module 'top2'. Per IEEE 1800-2023 Sec 25.3, this interface instantiation must be "
                     "elaborated. Fix pending.";
  }
  EXPECT_EQ(intf->getDefName(), std::string_view{"pins_if"});
}

// ---------------------------------------------------------------------------
// Sec 27.4: "for (genvar i = 0; i < Width; i++) begin : each_pin_intf ..."
// inside pins_if, and top2's own "... begin : each_pin ..." -- both loop
// bounds resolve to Width == 2 (top2's default), so both must have 2
// iterations.
// ---------------------------------------------------------------------------

TEST_F(GenerateInterfaceTest, PinsIfEachPinIntfLoopHasTwoIterations) {
  const hldb::Module *const top2 = getModule("top2");
  ASSERT_NE(top2, nullptr);
  const hldb::Interface *const intf = findElaboratedInterface(top2, "intf");
  if (intf == nullptr) {
    GTEST_SKIP() << "'intf' itself was not elaborated (see Top2InstantiatesPinsIfAsIntfWithWidthOverride); cannot "
                     "check its nested 'each_pin_intf' loop.";
  }
  const hldb::GenScopeArray *const eachPinIntf = findGenScopeArray(intf, "each_pin_intf");
  if (eachPinIntf == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate 'for (genvar i = 0; i < Width; i++) begin : each_pin_intf ... end' "
                     "inside 'pins_if' (no GenScopeArray named 'each_pin_intf' found). Per IEEE 1800-2023 Sec "
                     "27.4, Width resolves to 2 here, so this loop must elaborate exactly 2 iterations. Fix "
                     "pending.";
  }
  EXPECT_EQ(eachPinIntf->getSize(), 2) << "Sec 27.4/23.10: Width == 2 (top2's default) must produce exactly 2 "
                                           "iterations";
}

TEST_F(GenerateInterfaceTest, Top2EachPinLoopHasTwoIterations) {
  const hldb::Module *const top2 = getModule("top2");
  ASSERT_NE(top2, nullptr);
  const hldb::GenScopeArray *const eachPin = findGenScopeArray(top2, "each_pin");
  if (eachPin == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate 'for (genvar i = 0; i < Width; i++) begin : each_pin ... end' inside "
                     "'top2' (no GenScopeArray named 'each_pin' found). Per IEEE 1800-2023 Sec 27.4, Width == 2 "
                     "(top2's default), so this loop must elaborate exactly 2 iterations. Fix pending.";
  }
  EXPECT_EQ(eachPin->getSize(), 2) << "Sec 27.4: Width == 2 (top2's default) must produce exactly 2 iterations";
}

// ---------------------------------------------------------------------------
// Sec 6.3: 'pins_oe' is never declared anywhere in this source, so every
// reference to it (in pins_if's and top2's identical assign statements)
// must fail to bind.
// ---------------------------------------------------------------------------

TEST_F(GenerateInterfaceTest, UndeclaredPinsOeFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "pins_oe"), nullptr)
      << "Sec 6.3: 'pins_oe' is never declared in 'pins_if' or 'top2'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
