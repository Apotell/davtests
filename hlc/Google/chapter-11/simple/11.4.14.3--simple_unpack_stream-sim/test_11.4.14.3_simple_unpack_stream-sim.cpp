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

// Tests for 11.4.14.3--simple_unpack_stream-sim.sv (tags: 11.4.14.3)
//   module top(input [1:0] a, input [1:0] b, input [1:0] c, output [5:0] d);
//     assign d = {<<2 {a, b, c}};
//   endmodule
//
// Checked:
//   - design has module top with exactly 4 nets: "a"/"b"/"c" [1:0]
//     (input), "d" [5:0] (output), all vpiNetType wire, each RefTypespec ->
//     LogicTypespec with its own Range. Per IEEE 1800-2023 Sec
//     6.7/23.2.2.3: input ports always default to nets, and an output
//     port with no explicit data type also defaults to a net, so all
//     four being nets here is correct; module has no variables
//     (getVariables() is null)
//   - module has exactly 1 continuous assignment: lhs RefObj "d", rhs
//     Operation (vpiOpType=stream-rl, the '{<<...}' right-to-left streaming
//     operator) with 2 operands: Constant "2" (the slice size) and a
//     nested Operation (vpiOpType=concatenation) with 3 operands: RefObj
//     "a", RefObj "b", RefObj "c"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//   - no processes
//
// Not checked:
//   - this file is annotated "(without result verification)" and has no
//     $display assertions, so there is no runtime value to check even in
//     principle.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SimpleUnpackStreamSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.3--simple_unpack_stream-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----

TEST_F(SimpleUnpackStreamSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SimpleUnpackStreamSimTest, ModuleHasFourNetsWithExpectedRanges) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 4u);
  struct Expected {
    const char *name;
    const char *left;
    const char *right;
  };
  const Expected expected[4] = {{"a", "1", "0"}, {"b", "1", "0"}, {"c", "1", "0"}, {"d", "5", "0"}};
  for (const Expected &exp : expected) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(exp.name, top->getNets());
    ASSERT_NE(net, nullptr) << "net " << exp.name;
    EXPECT_EQ(net->getNetType(), vpiWire) << "net " << exp.name;
    const hldb::LogicTypespec *const lt = net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "net " << exp.name;
    ASSERT_NE(lt->getRanges(), nullptr);
    EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), exp.left) << "net " << exp.name;
    EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), exp.right) << "net " << exp.name;
  }
}

TEST_F(SimpleUnpackStreamSimTest, ModuleHasNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr) << "all four ports default to nets per IEEE "
                                              "1800-2023 Sec 6.7/23.2.2.3, so the module should "
                                              "have no variables";
}

// --- continuous assignment: right-to-left streaming operator ----

TEST_F(SimpleUnpackStreamSimTest, ContAssignIsStreamRlOfAbcWithSliceSizeTwo) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLhs<hldb::RefObj>()->getName(), "d");
  const hldb::Operation *const stream = ca->getRhs<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(stream->getOperands()->at(0))->getDecompile(), "2");
  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 3u);
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(1))->getName(), "b");
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(2))->getName(), "c");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(SimpleUnpackStreamSimTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SimpleUnpackStreamSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(SimpleUnpackStreamSimTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
