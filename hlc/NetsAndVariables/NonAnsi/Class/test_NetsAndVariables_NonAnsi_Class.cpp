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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Class.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the
// file-scope class testing point stands on its own.
//
// Checked:
//   - nets_and_variables_class_nonansi is reachable via Design::getAllClasses()
//   - cls_logic / cls_reg / cls_bits are hldb::Variable with the expected
//     typespecs
//
// Note: hldb::ClassDefn extends Scope, not Instance -- it has no getNets()
// accessor at all (nets are not standard class members per the LRM), so
// there is no "no Net duplicate" check to write here: the object model
// itself makes a class-scoped Net impossible to represent. Class body syntax
// does not differ between ANSI and non-ANSI style, so this mirrors the ANSI
// suite's Class test exactly.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class NonAnsiClassTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Class.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getCls() {
    return hldb::findByName<hldb::ClassDefn>("nets_and_variables_class_nonansi", m_design->getAllClasses());
  }
};

TEST_F(NonAnsiClassTest, ClassExists) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  ASSERT_NE(getCls(), nullptr);
}

TEST_F(NonAnsiClassTest, ClsLogicIsVariable) {
  const hldb::ClassDefn *const cls = getCls();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("cls_logic", cls->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(NonAnsiClassTest, ClsRegIsVariable) {
  const hldb::ClassDefn *const cls = getCls();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("cls_reg", cls->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(NonAnsiClassTest, ClsBitsIsBitTypespecVectorFour) {
  // 'bit [3:0] cls_bits;' has an explicit packed dimension -- confirm it is
  // modeled as a vector with range [3:0], not just an unranged BitTypespec.
  const hldb::ClassDefn *const cls = getCls();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("cls_bits", cls->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::BitTypespec *const bs = rts->getActual<hldb::BitTypespec>();
  ASSERT_NE(bs, nullptr);
  EXPECT_TRUE(bs->getVector());
  ASSERT_NE(bs->getRanges(), nullptr);
  ASSERT_EQ(bs->getRanges()->size(), 1u);
  EXPECT_EQ(bs->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(bs->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
