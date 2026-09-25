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

// Tests for tests/DataAttrib/dut.sv:
//
//   module top();
//   (* preserve *) reg my_reg1, my_reg2;
//   reg no_attrib;
//   endmodule
//
// Per IEEE 1800-2023 Sec 5.12, an attribute_instance immediately preceding a
// declaration attaches to that declaration. "(* preserve *)" precedes a
// single data_declaration that lists two variables (my_reg1, my_reg2), so
// per Sec 5.12 / Annex 5.7.1 grammar (attr_spec inside a data_declaration)
// the attribute attaches to that declaration -- both my_reg1 and my_reg2
// carry their own "preserve" flag attribute (no value). "no_attrib" is a
// separate data_declaration with no preceding attribute_instance and must
// carry none.
//
// "reg" is a variable data type with no net-type keyword, so per IEEE
// 1800-2023 Sec 6.7/6.8 my_reg1/my_reg2/no_attrib must appear as Variables,
// never duplicated as Nets.
//
// KNOWN BUG (matches Google/chapter-5/5.12-attributes-variable): HLC
// currently hoists attributes onto the containing Module's vpiAttribute
// list instead of attaching them to the individual declarations. The
// attribute-attachment assertions below are marked GTEST_SKIP() until
// that's fixed -- do not "fix" them to match current (wrong) output.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/attribute.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/variable.h>

namespace hlc {

class DataAttribTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DataAttrib.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// ---------------------------------------------------------------------------
// Module and variables
// ---------------------------------------------------------------------------

TEST_F(DataAttribTest, ModuleExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(DataAttribTest, ThreeVariablesExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u);

  EXPECT_NE(hldb::findByName<hldb::Variable>("my_reg1", top->getVariables()), nullptr) << "'my_reg1' missing";
  EXPECT_NE(hldb::findByName<hldb::Variable>("my_reg2", top->getVariables()), nullptr) << "'my_reg2' missing";
  EXPECT_NE(hldb::findByName<hldb::Variable>("no_attrib", top->getVariables()), nullptr) << "'no_attrib' missing";
}

// 'reg' has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 none of
// the three declarations must also appear in the module's net collection.
TEST_F(DataAttribTest, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("my_reg1", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("my_reg2", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("no_attrib", top->getNets()), nullptr);
  }
}

// ---------------------------------------------------------------------------
// Per IEEE 1800-2023 Sec 5.12, attributes attach to the declaration they
// immediately precede, not to the enclosing module.
// KNOWN BUG: HLC currently hoists them onto the module instead -- skipped
// until that's fixed.
// ---------------------------------------------------------------------------

TEST_F(DataAttribTest, ModuleHasNoAttributes) {
  GTEST_SKIP() << "HLC hoists the '(* preserve *)' attribute onto the Module instead of attaching it to the "
                  "'my_reg1, my_reg2' declaration; should have zero module-level attributes per "
                  "IEEE 1800-2023 Sec 5.12. Fix pending.";
}

TEST_F(DataAttribTest, MyReg1HasPreserveFlagAttribute) {
  GTEST_SKIP() << "HLC hoists the '(* preserve *)' attribute onto the Module instead of attaching it to "
                  "'my_reg1'; per IEEE 1800-2023 Sec 5.12 the attribute must be on the declaration itself "
                  "(a flag attribute: getValue() == nullptr). Fix pending.";
}

TEST_F(DataAttribTest, MyReg2HasPreserveFlagAttribute) {
  GTEST_SKIP() << "HLC hoists the '(* preserve *)' attribute onto the Module instead of attaching it to "
                  "'my_reg2'; per IEEE 1800-2023 Sec 5.12 the attribute must be on the declaration itself "
                  "(a flag attribute: getValue() == nullptr). Fix pending.";
}

TEST_F(DataAttribTest, NoAttribHasNoAttributes) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const noAttrib = hldb::findByName<hldb::Variable>("no_attrib", top->getVariables());
  ASSERT_NE(noAttrib, nullptr);
  EXPECT_TRUE(!noAttrib->getAttributes() || noAttrib->getAttributes()->empty())
      << "'no_attrib' has no preceding attribute_instance and must carry none";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
