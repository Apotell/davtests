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

// Tests for reverse.sv (tags: 7.12.2 7.4.2)
//   module top ();
//     string s[] = { "hello", "sad", "world" };
//     initial begin
//       $display(":assert: (('%s' == 'hello') and ('%s' == 'sad') and ('%s' == 'world'))",
//         s[0], s[1], s[2]);
//       s.reverse;
//       $display(":assert: (('%s' == 'world') and ('%s' == 'sad') and ('%s' == 'hello'))",
//         s[0], s[1], s[2]);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 1 variable: "s" (IEEE 1800-2023
//     6.7/6.8: 'string s[] = {...}' has no net-type keyword, so it is a
//     variable_declaration, not a net_declaration); it does not appear in
//     getNets()
//   - variable "s": RefTypespec -> ArrayTypespec vpiArrayType=dynamic(2),
//     elem -> StringTypespec; initial value stored directly on the
//     Variable as an Operation (vpiOpType=concatenation(33)) with 3 string
//     Constant operands "hello", "sad", "world"
//   - Initial process: 1 Begin with 3 stmts (SysFuncCall + HierPath +
//     SysFuncCall)
//   - Stmt[0]: $display with 4 args (format + BitSelect s[0], s[1], s[2])
//   - Stmt[1]: s.reverse (no parens) -- HierPath "s.reverse()" with 2 path
//     elems: RefObj "s" (resolving Variable "s") and MethodFuncCall
//     "reverse" with no arguments -- COMPILER BEHAVIOR: unlike the
//     parenthesis-less ".size"/".index" gap documented elsewhere in this
//     repo, "reverse"
//     (no parens, no arguments) correctly resolves to a MethodFuncCall
//     with zero errors
//   - Stmt[2]: $display with 4 args (format + BitSelect s[0], s[1], s[2],
//     documenting the post-reverse order)
//   - design-level typespecs (3): ModuleTypespec, StringTypespec,
//     IntTypespec (signed) -- NOTE: String comes before Int here (unlike
//     the standard Module/Int/String order elsewhere in this directory),
//     since "s"'s string-literal initializer is elaborated before any
//     int constant
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime contents of s after s.reverse -- simulation-only (see
//     the skipped canary RuntimeReverseResultRequiresSimulation below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackedReverseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "reverse.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variable ----

TEST_F(UnpackedReverseTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedReverseTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u)
      << "6.7/6.8: 'string s[] = {...}' declared with no net-type keyword is a variable";
}

TEST_F(UnpackedReverseTest, ModuleHasNoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr) << "no net-type keyword is present in reverse.sv";
}

TEST_F(UnpackedReverseTest, VarSIsDynamicArrayOfString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(UnpackedReverseTest, VarSInitialValueIsConcatenationOfThreeStrings) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::Operation *const init = s->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getOpType(), vpiConcatOp);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 3u);
  const std::string expected[3] = {"hello", "sad", "world"};
  for (uint32_t i = 0; i < 3u; ++i) {
    EXPECT_EQ(any_cast<hldb::Constant>(init->getOperands()->at(i))->getValue(), expected[i]) << "operand " << i;
  }
}

// --- initial process ----

TEST_F(UnpackedReverseTest, InitialBeginHasThreeStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

TEST_F(UnpackedReverseTest, FirstStmtDisplaysHelloSadWorld) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (('%s' == 'hello') and ('%s' == 'sad') and ('%s' == 'world'))");
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1);
    EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "s");
    EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
  }
}

TEST_F(UnpackedReverseTest, SecondStmtIsReverseHierPathWithNoErrorDespiteNoParens) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(1));
  ASSERT_NE(hp, nullptr) << "'s.reverse' should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(sRef, nullptr);
  EXPECT_NE(sRef->getActual<hldb::Variable>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'reverse' without parens should still resolve to a MethodFuncCall";
  EXPECT_EQ(call->getName(), "reverse");
  EXPECT_EQ(call->getArguments(), nullptr);
}

TEST_F(UnpackedReverseTest, ThirdStmtDisplaysWorldSadHello) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (('%s' == 'world') and ('%s' == 'sad') and ('%s' == 'hello'))");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(UnpackedReverseTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedReverseTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedReverseTest, DesignHasStringTypespecBeforeIntTypespec) {
  // NOTE: unlike sibling files in this directory, String comes before Int here.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(1)), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(2));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(UnpackedReverseTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedReverseTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: runtime reverse result requires simulation ----

TEST_F(UnpackedReverseTest, RuntimeReverseResultRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates reverse.sv; it does not run a simulator, "
                  "so the actual runtime contents of s after s.reverse cannot be observed here. "
                  "reverse.sv's own $display format string documents the expected values.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: (('%s' == 'world') and ('%s' == 'sad') and ('%s' == 'hello'))")
      << "expected s == {world, sad, hello} after s.reverse on {hello, sad, world}";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
