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

// Validates the HLDB model built for tests/PatternAssignLogic/dut.sv:
//
//   module top(/*output logic[7:0] o*/);
//      //assign o = '{default: 1};
//      parameter logic[7:0] P = '{default: 1};
//   endmodule
//
// The fixture gives a logic [7:0] parameter the assignment pattern
// '{default: 1} (10.9.1). The port and the assignment are commented out, so
// the list of ports is empty.
//
// What is checked, and why:
//   - top's '()' declares one null port (37.14 detail 10); the comment
//     inside it is not a port
//   - P is a parameter of type logic [7:0] (6.20.2) and is not local (6.20.1,
//     no parameter port list)
//   - P's value is an assignment pattern with one item, the key default with
//     value 1 (10.9.1)
//   - top holds no continuous assignment: the commented one is not code
//   - no fatal, syntax or error diagnostic: the file is legal
//
// Model convention: the 'default: 1' item is a TaggedPattern whose tag is a
// RefObj named "default" and whose pattern is the value.
//
// What is NOT checked, and why:
//   - The reduced value 8'hFF: no .hlc in this suite elaborates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PatternAssignLogicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PatternAssignLogic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }
};

TEST_F(PatternAssignLogicTest, TopHasOneNullPort) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "a null port has no name";
  EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10";
}

TEST_F(PatternAssignLogicTest, PIsOverridableLogic7To0) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("P", m->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: no parameter port list, so P is overridable";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.20.2: P is of type logic [7:0]";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstant(lt->getRanges()->at(0)->getLeftExpr(), "7", "left bound");
  ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), "0", "right bound");
}

TEST_F(PatternAssignLogicTest, PIsDefaultOnePattern) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  ASSERT_EQ(m->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = m->getParamAssigns()->at(0);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Parameter>("P", m->getParameters()));
  const hldb::Operation *const pat = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pat, nullptr);
  EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an assignment pattern";
  ASSERT_NE(pat->getOperands(), nullptr);
  ASSERT_EQ(pat->getOperands()->size(), 1u);
  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(pat->getOperands()->at(0));
  ASSERT_NE(tp, nullptr);
  const hldb::RefObj *const key = tp->getTag<hldb::RefObj>();
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->getName(), "default") << "10.9.1: the default key";
  ExpectConstant(tp->getPattern(), "1", "10.9.1: the default value");
}

TEST_F(PatternAssignLogicTest, CommentedAssignmentIsNotModeled) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE((m->getContAssigns() == nullptr) || m->getContAssigns()->empty()) << "5.4: a comment is not code";
}

TEST_F(PatternAssignLogicTest, NoFatalSyntaxOrErrorDiagnostics) {
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
