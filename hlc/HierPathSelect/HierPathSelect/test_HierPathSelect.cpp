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

// Tests for tests/HierPathSelect/dut.sv:
//
//   module dut2 #() ();
//   typedef struct packed {
//     logic [5:0] q;
//   } struct_typedef;
//   struct_typedef read_buf;
//
//   assign read_buf.q[1] = 1'b1;
//   endmodule
//
// "read_buf.q[1]" reaches the packed field "q" of "read_buf" via a
// hierarchical path (RefObj "read_buf" -> member "q") and then bit-selects
// one bit of that field with "[1]" -- the select is applied *after* the
// hierarchical path, the same ordering as HierPathPackedStruct's
// "a.pair[0]" (contrast with HierPathPackedArrayNet/HierPathPackedVar,
// where the select is applied to the base name before the hierarchical
// descent).
//
// Note: "read_buf" carries no net-type keyword, so per IEEE 1800-2023
// Sec 6.7/6.8 it must be modeled as a Variable. A continuous assignment's
// LHS must be a net (or a select/concatenation of nets) per Sec 10.3.2 --
// assigning to a Variable via "assign" is therefore illegal even though it
// parses; this test does not assert anything about whether the assignment
// is flagged as an error, only the parse-time hierarchical-path/select
// shape, which the grammar guarantees regardless.
//
// Checked:
//   - module "dut2" exists, has one Variable "read_buf" (not a Net) typed
//     via a packed struct typespec with a single 6-bit field "q"
//   - module has exactly one ContAssign whose LHS is
//     BitSelect{ prefix: hierarchical RefObj "read_buf.q" with path
//     elements ["read_buf", "q"], index: 1 }, RHS is Constant "1"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace hlc {

class HierPathSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathSelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut2() {
    return hldb::findByDefName<hldb::Module>("dut2", m_design->getAllModules());
  }
};

TEST_F(HierPathSelectTest, ModuleExists) { EXPECT_NE(getDut2(), nullptr); }

TEST_F(HierPathSelectTest, ReadBufIsVariableNotNet) {
  // Per IEEE 1800-2023 Sec 6.7/6.8: no net-type keyword means "read_buf"
  // must be modeled as a Variable, never a Net.
  const hldb::Module *const mod = getDut2();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const asVar = hldb::findByName<hldb::Variable>("read_buf", mod->getVariables());
  EXPECT_NE(asVar, nullptr) << "'read_buf' has no net-type keyword and must be modeled as a Variable";
  const hldb::Net *const asNet = hldb::findByName<hldb::Net>("read_buf", mod->getNets());
  EXPECT_EQ(asNet, nullptr) << "'read_buf' must not be modeled as a Net (no net-type keyword given)";
}

TEST_F(HierPathSelectTest, HasOneContAssign) {
  const hldb::Module *const mod = getDut2();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 1u);
}

TEST_F(HierPathSelectTest, LhsIsHierPathThenBitSelect) {
  const hldb::Module *const mod = getDut2();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_EQ(mod->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = mod->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  ASSERT_NE(ca->getLhs(), nullptr);
  const hldb::BitSelect *const bsel = ca->getLhs<hldb::BitSelect>();
  ASSERT_NE(bsel, nullptr) << "'read_buf.q[1]' LHS should be a BitSelect";

  ASSERT_NE(bsel->getPrefix(), nullptr);
  const hldb::RefObj *const prefix = bsel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix should be the hierarchical path RefObj 'read_buf.q'";
  EXPECT_EQ(prefix->getName(), std::string_view{"read_buf.q"});
  ASSERT_NE(prefix->getPathElems(), nullptr);
  ASSERT_EQ(prefix->getPathElems()->size(), 2u);
  EXPECT_EQ(prefix->getPathElems()->at(0)->getName(), std::string_view{"read_buf"});
  EXPECT_EQ(prefix->getPathElems()->at(1)->getName(), std::string_view{"q"});

  ASSERT_NE(bsel->getIndex(), nullptr);
  const hldb::Constant *const idx = bsel->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(std::string(idx->getValue()), "1");

  ASSERT_NE(ca->getRhs(), nullptr);
  const hldb::Constant *const rhs = ca->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(std::string(rhs->getValue()), "1");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
