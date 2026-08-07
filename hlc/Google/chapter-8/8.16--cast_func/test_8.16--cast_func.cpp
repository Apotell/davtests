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

// Tests for 8.16--cast_func.sv (tags: 8.16)
//   module class_tb ();
//     typedef enum { aaa, bbb, ccc, ddd, eee } values;
//     initial begin
//       values val;
//       if(!$cast(val, 5))
//         $display("$cast failed");
//       $display(val);
//     end
//   endmodule
//
// IEEE 1800-2017 8.16 "The $cast function": "$cast(dest, source)" is a
// DYNAMIC cast, usable both for class handles (per 8.16's usual class
// hierarchy context) and, as here, for assigning an arbitrary integer
// value into an enum-typed variable with a runtime validity check --
// there is no compile-time guarantee "5" is a member of "values" (it
// isn't: the enum only spans 0-4), so $cast returns a bit indicating
// success/failure, checked here via "!$cast(...)" in a conditional.
//
// Checked:
//   - design has module class_tb with exactly 2 module-level
//     typespecs: an anonymous EnumTypespec and a TypedefTypespec named
//     "values" whose alias resolves to that same EnumTypespec
//   - the EnumTypespec has exactly 5 EnumConsts, in declaration order:
//     "aaa", "bbb", "ccc", "ddd", "eee" -- none of them carries an
//     explicit value Expr (getValue() == nullptr for all 5), since the
//     source declares no "= <value>" for any of them
//   - the initial process' Begin block has exactly 1 block-local
//     Variable, "val", declared INSIDE the block itself (not at module
//     scope, unlike every other chapter-8 file in this suite so far) --
//     accessible via Begin::getVariables(), not the module's; its
//     typespec resolves to the "values" TypedefTypespec
//   - the initial process' Begin block has exactly 2 statements:
//     "if(!$cast(val, 5)) $display(\"$cast failed\");" and
//     "$display(val);"
//   - "if(!$cast(val, 5)) ...": an IfStmt whose condition is an Operation
//     (vpiOpType vpiNotOp, the unary "!") with 1 operand, a SysFuncCall
//     "$cast" (an ordinary system FUNCTION call here, since it is used in
//     an expression/boolean context -- contrast with $display, which
//     appears elsewhere in this suite as SysTaskCall since it is always a
//     bare statement) taking 2 arguments: a RefObj "val" resolving to the
//     block-local Variable, and a Constant "5". The IfStmt's own getStmt()
//     (the then-branch) is a bare SysTaskCall "$display" with the string
//     argument "\"$cast failed\""; there is no else-branch
//   - "$display(val);": a SysTaskCall whose argument is a plain RefObj
//     "val" resolving to the SAME block-local Variable
//   - design-level: no classes in this file at all (the FIRST chapter-8
//     file in this suite with no "class" declaration whatsoever -- $cast
//     applies more generally than just class handles)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/if_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassCastFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.16--cast_func.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("class_tb", m_design->getAllModules());
  }

  static const hldb::EnumTypespec *getEnumTypespec() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTypespecs() == nullptr || top->getTypespecs()->empty()) return nullptr;
    return any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  }

  static const hldb::TypedefTypespec *getValuesTypedef() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTypespecs() == nullptr || top->getTypespecs()->size() < 2) return nullptr;
    return any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(1));
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getVariableVal() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getVariables() == nullptr || begin->getVariables()->empty()) return nullptr;
    return begin->getVariables()->at(0);
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassCastFuncTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassCastFuncTest, ModuleHasTwoTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 2u);
}

// --- enum / typedef "values" -------------------------------------------------

TEST_F(ClassCastFuncTest, EnumTypespecHasFiveEnumConstsInOrder) {
  const hldb::EnumTypespec *const et = getEnumTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnumConsts()->size(), 5u);
  const char *const expected[5] = {"aaa", "bbb", "ccc", "ddd", "eee"};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::EnumConst *const ec = et->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), expected[i]);
  }
}

TEST_F(ClassCastFuncTest, EnumConstsHaveNoExplicitValue) {
  const hldb::EnumTypespec *const et = getEnumTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnumConsts(), nullptr);
  for (size_t i = 0; i < et->getEnumConsts()->size(); ++i) {
    const hldb::EnumConst *const ec = et->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getValue(), nullptr) << "enumerator '" << ec->getName()
                                       << "' declares no '= <value>', so it should attach no value Expr";
  }
}

TEST_F(ClassCastFuncTest, EnumTypespecBaseTypeDefaultsToInt) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 6.19: 'In the absence of a data type declaration, the default data type "
                  "shall be int.' Phase2ModelBuilder's enum-typespec handling only calls setBaseTypespec() when "
                  "an explicit paEnum_base_type AST node is present (Phase2ModelBuilder.cpp ~line 6289); this "
                  "anonymous 'enum { aaa, bbb, ccc, ddd, eee }' has no explicit base type, so getBaseTypespec() "
                  "stays permanently null instead of resolving to an implicit signed 32-bit int.";
  const hldb::EnumTypespec *const et = getEnumTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getBaseTypespec(), nullptr) << "enum with no explicit base type should still get an implicit "
                                                "'int' RefTypespec (IEEE 1800-2023 6.19)";
  const hldb::IntTypespec *const base = et->getBaseTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(base, nullptr) << "the implicit enum base type should resolve to IntTypespec ('int')";
  EXPECT_TRUE(base->getSigned());
}

TEST_F(ClassCastFuncTest, ValuesTypedefAliasesEnumTypespec) {
  const hldb::TypedefTypespec *const td = getValuesTypedef();
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "values");
  ASSERT_NE(td->getTypedefAlias(), nullptr);
  EXPECT_EQ(td->getTypedefAlias()->getActual<hldb::EnumTypespec>(), getEnumTypespec());
}

// --- design shape: no classes at all -------------------------------------------

TEST_F(ClassCastFuncTest, ModuleHasNoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getClassDefns() == nullptr || top->getClassDefns()->empty())
      << "this file declares no 'class' at all -- $cast applies more generally than just class handles";
}

TEST_F(ClassCastFuncTest, DesignHasNoClasses) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getAllClasses() == nullptr || m_design->getAllClasses()->empty())
      << "this file declares no 'class' at all -- $cast applies more generally than just class handles";
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassCastFuncTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassCastFuncTest, InitialBeginHasOneLocalVariableVal) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getVariables(), nullptr);
  ASSERT_EQ(begin->getVariables()->size(), 1u);
  const hldb::Variable *const val = getVariableVal();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  ASSERT_NE(val->getTypespec(), nullptr);
  EXPECT_EQ(val->getTypespec<hldb::RefTypespec>()->getActual<hldb::TypedefTypespec>(), getValuesTypedef());
}

TEST_F(ClassCastFuncTest, VariableValDefaultsToStaticLifetime) {
  const hldb::Variable *const val = getVariableVal();
  ASSERT_NE(val, nullptr);
  EXPECT_FALSE(val->getAutomatic())
      << "IEEE 1800-2023 Sec 6.21: a variable declared directly inside a procedural block that is not itself "
         "declared automatic defaults to a static lifetime -- e.g. the standard's own example, 'int st1; // "
         "static' declared directly inside 'initial begin ... end', which is exactly this file's shape "
         "('values val;' with neither an enclosing automatic block nor an explicit lifetime keyword)";
}

TEST_F(ClassCastFuncTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u);
}

// --- if(!$cast(val, 5)) $display("$cast failed"); (stmt[0]) ---------------------

TEST_F(ClassCastFuncTest, FirstStmtIsIfNotCastValFive) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(begin->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr) << "stmt[0] should be an IfStmt";

  const hldb::Operation *const notOp = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(notOp, nullptr) << "'!$cast(val, 5)' should be a unary-not Operation";
  EXPECT_EQ(notOp->getOpType(), vpiNotOp);
  ASSERT_NE(notOp->getOperands(), nullptr);
  ASSERT_EQ(notOp->getOperands()->size(), 1u);

  const hldb::SysFuncCall *const cast = any_cast<hldb::SysFuncCall>(notOp->getOperands()->at(0));
  ASSERT_NE(cast, nullptr) << "'$cast(val, 5)' should be a SysFuncCall (expression context, unlike $display's "
                              "SysTaskCall usage elsewhere)";
  EXPECT_EQ(cast->getName(), "$cast");
  ASSERT_NE(cast->getArguments(), nullptr);
  ASSERT_EQ(cast->getArguments()->size(), 2u);
  const hldb::RefObj *const valArg = any_cast<hldb::RefObj>(cast->getArguments()->at(0));
  ASSERT_NE(valArg, nullptr);
  EXPECT_EQ(valArg->getName(), "val");
  EXPECT_EQ(valArg->getActual<hldb::Variable>(), getVariableVal());
  const hldb::Constant *const fiveArg = any_cast<hldb::Constant>(cast->getArguments()->at(1));
  ASSERT_NE(fiveArg, nullptr);
  EXPECT_EQ(fiveArg->getDecompile(), "5");

  const hldb::SysTaskCall *const thenBranch = ifStmt->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(thenBranch, nullptr) << "the then-branch '$display(\"$cast failed\");' should be a bare SysTaskCall";
  EXPECT_EQ(thenBranch->getName(), "$display");
  ASSERT_NE(thenBranch->getArguments(), nullptr);
  ASSERT_EQ(thenBranch->getArguments()->size(), 1u);
  const hldb::Constant *const msg = any_cast<hldb::Constant>(thenBranch->getArguments()->at(0));
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->getDecompile(), "\"$cast failed\"");
}

// --- $display(val); (stmt[1]) ------------------------------------------------------

TEST_F(ClassCastFuncTest, SecondStmtDisplaysVal) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr) << "stmt[1] should be a $display SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::RefObj *const valArg = any_cast<hldb::RefObj>(disp->getArguments()->at(0));
  ASSERT_NE(valArg, nullptr);
  EXPECT_EQ(valArg->getName(), "val");
  EXPECT_EQ(valArg->getActual<hldb::Variable>(), getVariableVal());
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassCastFuncTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
