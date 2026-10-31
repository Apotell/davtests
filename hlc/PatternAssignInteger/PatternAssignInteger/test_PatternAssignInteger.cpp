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

// Validates the HLDB model built for tests/PatternAssignInteger/dut.sv:
//
//   module top(output logic[7:0] o);
//      assign o = '{0:1, 3:1, 7:1, default:0};
//   endmodule
//
// The fixture assigns an array assignment pattern with index keys and a
// default (10.9.1) to a packed vector. 10.9 shows the same form for an
// integral variable: "var integer i = '{31:1, 23:1, 15:1, 8:1, default:0};".
//
// What is checked, and why:
//   - o is the only port, an output variable of type logic [7:0]
//     (23.2.2.3, 7.4.1)
//   - the continuous assignment (10.3) targets o
//   - its RHS is an assignment pattern (10.9.1) with 4 items in source
//     order: index keys 0, 3 and 7 with value 1, then default with value 0
//   - no fatal, syntax or error diagnostic: the file is legal
//
// Model convention: each 'key:value' item is a TaggedPattern whose tag is
// the key (a Constant for an index, a RefObj named "default" for default)
// and whose pattern is the value.
//
// What is NOT checked, and why:
//   - The resulting value 8'b10001001: reducing the pattern is elaboration,
//     and no .hlc in this suite elaborates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PatternAssignIntegerTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PatternAssignInteger.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getTop();
    if ((m == nullptr) || (m->getContAssigns() == nullptr) || (m->getContAssigns()->size() != 1)) return nullptr;
    return m->getContAssigns()->at(0);
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }
};

TEST_F(PatternAssignIntegerTest, OIsLogic7To0OutputVariable) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "o");
  EXPECT_EQ(m->getPorts()->at(0)->getDirection(), vpiOutput);
  const hldb::Variable *const o = hldb::findByName<hldb::Variable>("o", m->getVariables());
  ASSERT_NE(o, nullptr) << "23.2.2.3: 'output logic' declares a variable";
  ASSERT_NE(o->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = o->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstant(lt->getRanges()->at(0)->getLeftExpr(), "7", "7.4.1: left bound");
  ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), "0", "7.4.1: right bound");
}

TEST_F(PatternAssignIntegerTest, ContAssignTargetsO) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr) << "10.3: one continuous assignment";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("o", getTop()->getVariables()));
}

TEST_F(PatternAssignIntegerTest, RhsIsIndexKeyedPatternWithDefault) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const pat = ca->getRhs<hldb::Operation>();
  ASSERT_NE(pat, nullptr);
  EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an assignment pattern";
  ASSERT_NE(pat->getOperands(), nullptr);
  ASSERT_EQ(pat->getOperands()->size(), 4u) << "10.9.1: 4 key:value items";
  const char *const keys[] = {"0", "3", "7"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(pat->getOperands()->at(i));
    ASSERT_NE(tp, nullptr) << "item " << i << " is key:value";
    ExpectConstant(tp->getTag(), keys[i], "10.9.1: the index key");
    ExpectConstant(tp->getPattern(), "1", "10.9.1: the value");
  }
  const hldb::TaggedPattern *const def = any_cast<hldb::TaggedPattern>(pat->getOperands()->at(3));
  ASSERT_NE(def, nullptr);
  const hldb::RefObj *const key = def->getTag<hldb::RefObj>();
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->getName(), "default") << "10.9.1: the default key";
  EXPECT_EQ(key->getActual(), nullptr) << "a keyword binds to no declaration";
  ExpectConstant(def->getPattern(), "0", "10.9.1: the default value");
}

TEST_F(PatternAssignIntegerTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
