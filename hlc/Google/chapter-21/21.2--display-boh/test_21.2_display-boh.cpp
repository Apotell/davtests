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

// Tests for 21.2--display-boh.sv (tags: 21.2)
//
//   module top();
//
//   initial begin
//     int val = 1234;
//     $displayb(val);
//     $displayo(val);
//     $displayh(val);
//   end
//
//   endmodule
//
// The "boh" in the fixture name stands for Binary / Octal / Hex. This file
// is the radix-variant counterpart of 21.2--display.sv: same module, same
// single "int val = 1234" declaration, but the one "$display(val)" call is
// replaced by the three default-radix variants.
//
// The corner this fixture exercises is IEEE 1800-2023 Sec 21.2.1 and Table
// 21-1: "$displayb", "$displayo" and "$displayh" are three separate,
// independently named system tasks, not "$display" carrying some radix
// modifier. They behave exactly like "$display" except that an argument
// with no format specification is printed in binary / octal / hexadecimal
// respectively, instead of in the default decimal radix that plain
// "$display" would use.
//
// What that means for a static model is the point of this test: the radix
// lives *in the task's name*, so the three calls must survive elaboration
// as three distinct SysTaskCall nodes with three distinct name strings. If
// the front end ever normalized the display family down to a single
// "$display" spelling, the three calls would become indistinguishable and
// the radix information would be lost outright -- so pinning each name to
// its exact spelling is the substantive check here, not boilerplate.
//
// What is checked:
//   - module "top" exists and is named "top"; it has an empty port list
//     and no end label (plain "endmodule", no repeated ": top"); it has no
//     nets and no module-level variables, because the file's only
//     declaration sits inside the initial block's begin-end (IEEE
//     1800-2023 Sec 6.3: such data is scoped to the enclosing block, not
//     to the module)
//   - "top" has exactly one process, an Initial whose statement is a Begin
//   - that Begin is unnamed on both ends (bare "begin" / bare "end", no
//     ": label" on either), and it declares exactly one variable, "val":
//     RefTypespec -> IntTypespec, signed and rangeless (IEEE 1800-2023 Sec
//     6.11.2: "int" is a signed 32-bit integer type, and "int val" carries
//     no explicit packed dimensions), with a declaration-time
//     getValue<Constant>() decompiling to "1234"
//   - the Begin has exactly 3 statements, in source order, each a
//     SysTaskCall named exactly "$displayb", "$displayo" and "$displayh"
//   - each of the three takes exactly 1 argument: a RefObj named "val"
//     whose getActual<Variable>() is the very same Variable object
//     declared above -- i.e. all three calls resolve to one shared
//     declaration, rather than each re-binding a fresh copy
//   - design-level typespecs (2): ModuleTypespec "top" and the single
//     shared signed IntTypespec behind "val". There is no string literal
//     anywhere in this file -- none of the three calls passes a format
//     string -- so no StringTypespec is created, unlike the sibling tests
//     that $display a literal
//   - compiler emits zero fatals, syntax errors, errors and warnings
//
// What is NOT checked and why:
//   - the actual binary / octal / hexadecimal rendering of 1234 (that is,
//     "10011010010", "2322" and "4d2"). Which radix each task prints in is
//     a runtime formatting property of the simulator's output stream. HLC
//     is a static compiler/elaborator and is not a simulator: it has no
//     console to capture here, and the SysTaskCall nodes carry only their
//     name strings and argument lists. The three calls are therefore
//     structurally identical to one another in the static model, and the
//     name string is the only place the radix is observable. This is out
//     of scope permanently rather than a gap waiting to be filled, so no
//     placeholder test is carried for it;
//     ThreeCallsKeepTheirDistinctRadixTaskNamesInSourceOrder is the
//     static evidence that the three variants stayed distinct.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>

namespace hlc {

class DisplayBohTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.2--display-boh.hlc"}); }

  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if ((top == nullptr) || (top->getProcesses() == nullptr) || top->getProcesses()->empty()) {
      return nullptr;
    }

    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // Shared shape check for the three radix variants: "$display<r>(val)" is
  // a SysTaskCall with exactly one argument, a RefObj that resolves to the
  // block-scoped "val" rather than to a re-typed literal or a fresh copy.
  static void expectDisplaysVal(const hldb::Any *stmt, std::string_view expectedName,
                                const hldb::Variable *expectedActual) {
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(stmt);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), expectedName);

    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);

    const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getName(), "val");
    EXPECT_EQ(arg->getActual<hldb::Variable>(), expectedActual)
        << expectedName << "(val) should bind to the 'val' declared in the same begin-end block";
  }
};

// ---- module ----

TEST_F(DisplayBohTest, ModuleTopExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getName(), "top");
}

TEST_F(DisplayBohTest, ModuleHasEmptyPortListAndNoEndLabel) {
  // "module top()" is the empty port-list form, and the file closes with a
  // plain "endmodule" -- no ": top" repeated -- so the module carries
  // neither ports nor an end label.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getPorts(), nullptr) << "'module top()' declares no ports";
  EXPECT_EQ(top->getEndLabel(), "") << "the source closes with a plain 'endmodule', no ': top' repeated";
}

TEST_F(DisplayBohTest, ModuleHasNoNetsAndNoModuleLevelVariables) {
  // The file's only declaration, "int val", is written inside the initial
  // block's begin-end, so it belongs to that Begin's scope and never
  // reaches the module (IEEE 1800-2023 Sec 6.3, "Variable declarations").
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);

  EXPECT_EQ(top->getNets(), nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
}

TEST_F(DisplayBohTest, ModuleHasExactlyOneInitialProcessWithABeginBody) {
  // "initial begin ... end" -- one process, and because the body is a
  // multi-statement begin-end it is a Begin rather than a bare statement.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::Begin>(), nullptr);
}

TEST_F(DisplayBohTest, InitialBeginBlockIsUnnamedOnBothEnds) {
  // Bare "begin" and bare "end", no ": label" written on either -- so per
  // IEEE 1800-2023 Sec 9.3.4 the block's name and end label are both empty.
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  EXPECT_TRUE(blk->getName().empty()) << "no ': label' follows 'begin', but got: " << blk->getName();
  EXPECT_EQ(blk->getEndLabel(), "") << "no ': label' repeats after 'end' either";
}

// ---- declaration: int val = 1234; ----

TEST_F(DisplayBohTest, InitialBlockDeclaresSignedIntValInitializedTo1234) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);

  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", blk->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");

  const hldb::IntTypespec *const intTypespec = val->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTypespec, nullptr);
  EXPECT_TRUE(intTypespec->getSigned()) << "IEEE 1800-2023 Sec 6.11.2: 'int' is a signed 32-bit integer type";
  EXPECT_EQ(intTypespec->getRanges(), nullptr) << "'int val' declares no explicit packed dimensions";

  ASSERT_NE(val->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(val->getValue<hldb::Constant>()->getDecompile(), "1234");
}

// ---- the three radix variants: $displayb / $displayo / $displayh ----

TEST_F(DisplayBohTest, InitialBlockHasExactlyThreeStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 3u) << "the begin-end body holds $displayb, $displayo and $displayh";
}

TEST_F(DisplayBohTest, ThreeCallsKeepTheirDistinctRadixTaskNamesInSourceOrder) {
  // This is the heart of the fixture. Per IEEE 1800-2023 Table 21-1 the
  // radix is carried by the task name itself, so "$displayb", "$displayo"
  // and "$displayh" must stay three distinct names in source order. Were
  // they folded into a single "$display" spelling, the binary / octal /
  // hex intent of the source would be unrecoverable from the database.
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);

  const hldb::SysTaskCall *const first = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  const hldb::SysTaskCall *const second = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  const hldb::SysTaskCall *const third = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(2));

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(third, nullptr);

  EXPECT_EQ(first->getName(), "$displayb");
  EXPECT_EQ(second->getName(), "$displayo");
  EXPECT_EQ(third->getName(), "$displayh");
}

TEST_F(DisplayBohTest, EachRadixCallIsNotUserDefined) {
  // 21.2: $displayb/o/h are standard system tasks defined by the language,
  // and this file registers no user-defined system task of its own.
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);

  for (std::size_t i = 0; i < 3u; ++i) {
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(i));
    ASSERT_NE(call, nullptr);
    EXPECT_FALSE(call->getUserDefn());
  }
}

TEST_F(DisplayBohTest, EachRadixCallPassesTheSingleSharedValDeclaration) {
  // All three calls name the same "val". Each argument must resolve back
  // to the one Variable object declared in this block -- one declaration
  // shared by three references, not three independent bindings.
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);

  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", blk->getVariables());
  ASSERT_NE(val, nullptr);

  expectDisplaysVal(blk->getStmts()->at(0), "$displayb", val);
  expectDisplaysVal(blk->getStmts()->at(1), "$displayo", val);
  expectDisplaysVal(blk->getStmts()->at(2), "$displayh", val);
}

// ---- design-level typespecs / compiler diagnostics ----

TEST_F(DisplayBohTest, DesignHasModuleAndIntTypespecsOnly) {
  // None of the three calls passes a format string -- each takes a bare
  // expression -- so this file contains no string literal at all and
  // therefore no StringTypespec. What remains is the module itself and the
  // single "int" data type behind "val", shared by all three references.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_EQ(m_design->getTypespecs()->size(), 2u);

  const hldb::ModuleTypespec *const moduleTypespec = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(moduleTypespec, nullptr);
  EXPECT_EQ(moduleTypespec->getName(), "top");

  const hldb::IntTypespec *const intTypespec = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(intTypespec, nullptr);
  EXPECT_TRUE(intTypespec->getSigned());
  EXPECT_EQ(intTypespec->getRanges(), nullptr);
}

TEST_F(DisplayBohTest, CompilerReportsZeroErrors) {
  // Nothing in this fixture is questionable: three well-formed calls to
  // three standard system tasks, each with a legal single argument.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);

  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
