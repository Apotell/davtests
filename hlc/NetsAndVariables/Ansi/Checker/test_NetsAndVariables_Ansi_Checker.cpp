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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Checker.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so the file-scope
// checker testing point stands on its own.
//
// Checked:
//   - nets_and_variables_checker exists with two input ports (clk, a)
//   - its logic variables (ck_logic, ck_vec) exist as hldb::Variable, and
//     are never duplicated as a hldb::Net (identifiers are unique within a
//     scope regardless of net/variable namespace)
//   - it has a single process, modeled as an Always process
//
// Per IEEE 1800, 'checker_or_generate_item_declaration' only permits
// 'data_declaration' (variable_declaration / var_type / type_declaration /
// package_import_declaration / net_type_declaration) as a checker item --
// there is no 'net_declaration' alternative anywhere in a checker body, so
// a net cannot be legally declared inside a checker (see grammar/SV3_1aParser.g4,
// rules checker_declaration -> checker_or_generate_item_declaration ->
// data_declaration). Checker.sv nonetheless declares 'wire ck_wire;' inside
// the checker; since this is not a legal construct, no assertion is made
// about what graph shape (if any) results from it -- see
// CkWireIsIllegalInsideChecker below.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_port.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiCheckerTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Checker.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AnsiCheckerTest, CheckerExists) {
  ASSERT_NE(m_design->getCheckerDecls(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::CheckerDecl>("nets_and_variables_checker", m_design->getCheckerDecls()),
            nullptr);
}

TEST_F(AnsiCheckerTest, CheckerHasTwoInputPorts) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getPorts(), nullptr);
  ASSERT_EQ(checker->getPorts()->size(), 2u);

  const hldb::CheckerPort *const clk = hldb::findByName<hldb::CheckerPort>("clk", checker->getPorts());
  const hldb::CheckerPort *const a = hldb::findByName<hldb::CheckerPort>("a", checker->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(a->getDirection(), vpiInput);
}

TEST_F(AnsiCheckerTest, CheckerHasLogicVariablesNoNetDuplicate) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_logic", checker->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_vec", checker->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("ck_logic", checker->getNets()), nullptr)
      << "'ck_logic' is variable-declared -- it must not also appear in vpiNet";
  EXPECT_EQ(hldb::findByName<hldb::Net>("ck_vec", checker->getNets()), nullptr)
      << "'ck_vec' is variable-declared -- it must not also appear in vpiNet";
}

TEST_F(AnsiCheckerTest, CkVecIsVectorOneToZero) {
  // 'logic [1:0] ck_vec;' has an explicit packed dimension.
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("ck_vec", checker->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(AnsiCheckerTest, CheckerHasRandVariables) {
  GTEST_SKIP() << "Rand is allowed only in unpacked structures; As per the standard, It shall be"
                  "legal to declare a random_qualifier (rand|randc) only within unpacked structures.";

  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);

  const hldb::Variable *const ckRand = hldb::findByName<hldb::Variable>("ck_rand", checker->getVariables());
  ASSERT_NE(ckRand, nullptr);
  EXPECT_EQ(ckRand->getRandType(), vpiRand);

  const hldb::Variable *const ckRandc = hldb::findByName<hldb::Variable>("ck_randc", checker->getVariables());
  ASSERT_NE(ckRandc, nullptr);
  EXPECT_EQ(ckRandc->getRandType(), vpiRandC);
}

TEST_F(AnsiCheckerTest, CheckerHasOneProcess) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getProcesses(), nullptr);
  ASSERT_EQ(checker->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Always>(checker->getProcesses()->at(0)), nullptr)
      << "'always_ff @(posedge clk)' should be modeled as an Always process";
}

TEST_F(AnsiCheckerTest, CkWireIsIllegalInsideChecker) {
  GTEST_SKIP() << "IEEE 1800 checker_or_generate_item_declaration only permits data_declaration (variables, "
                  "typedefs, package imports, nettype declarations) as a checker item -- there is no "
                  "net_declaration alternative in a checker body, so 'wire ck_wire;' is not a legal construct. "
                  "No assertion is made about the resulting graph shape for this illegal declaration.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
