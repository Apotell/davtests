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

// Tests for tests/DatQuery01/dut.sv:
//
//   module tb;
//     int i;
//     logic [7:0] l8;
//     real r;
//     string s = "test";
//     int bit_count;
//     string type_name;
//     initial begin
//       bit_count = $bits(i);
//       bit_count = $bits(l8);
//       bit_count = $bits(real);
//       type_name = $typename(s);
//       type_name = $typename(i);
//       type_name = $typename(logic);
//     end
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.2 defines '$bits(expression|type)' as a constant
// system function returning the number of bits required to hold the
// argument; Sec 20.6.1 (via the "Typename" system function, Sec 20.6.1
// in some LRM printings is 'string type name' functions) defines
// '$typename(expression|type)' as returning a string naming the argument's
// type. Both accept either an expression (queried for its type) or a
// type name/keyword written directly.
//
// Checked:
//   - module 'tb' exists with its six variables
//   - the initial block's Begin has exactly 6 blocking assignment statements
//   - $bits(i), $bits(l8): SysFuncCall "$bits" with one argument, a RefObj
//     naming the queried variable and resolving to its actual declaration
//   - $bits(real): SysFuncCall "$bits" with one argument (a bare type name,
//     not a variable reference -- checked only for existence, since its
//     concrete node shape is not asserted here)
//   - $typename(s), $typename(i): SysFuncCall "$typename" with one
//     argument, a RefObj naming the queried variable
//   - $typename(logic): SysFuncCall "$typename" with one argument (a bare
//     type keyword -- existence only, same reasoning as $bits(real))
//   - compiler reports zero errors (this is a straightforwardly legal file)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>

#include <cstddef>
#include <string>

namespace hlc {

class DatQuery01Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DatQuery01.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTb() { return hldb::findByName<hldb::Module>("tb", m_design->getAllModules()); }

  static const hldb::Begin *getBegin() {
    const hldb::Module *const m = getTb();
    if (m == nullptr || m->getProcesses() == nullptr || m->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>((*m->getProcesses())[0]);
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAssignment(std::size_t index) {
    const hldb::Begin *const begin = getBegin();
    if (begin == nullptr || begin->getStmts() == nullptr) return nullptr;
    if (index >= begin->getStmts()->size()) return nullptr;
    return any_cast<hldb::Assignment>((*begin->getStmts())[index]);
  }
};

// ---------------------------------------------------------------------------
// Module and variables
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, ModuleExists) { ASSERT_NE(getTb(), nullptr) << "module 'tb' not found"; }

TEST_F(DatQuery01Test, SixVariablesExist) {
  const hldb::Module *const tb = getTb();
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getVariables(), nullptr);
  EXPECT_EQ(tb->getVariables()->size(), 6u)
      << "expected 6 variables: i, l8, r, s, bit_count, type_name";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, InitialBlockHasBegin) { ASSERT_NE(getBegin(), nullptr); }

TEST_F(DatQuery01Test, BeginHasSixStatements) {
  const hldb::Begin *const begin = getBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 6u);
}

TEST_F(DatQuery01Test, AllAssignmentsAreBlocking) {
  const hldb::Begin *const begin = getBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "assignment[" << i << "] should be blocking (=)";
  }
}

// ---------------------------------------------------------------------------
// bit_count = $bits(i)
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, BitsOfI_IsSysFuncCallWithOneRefObjArgument) {
  const hldb::Assignment *const assign = getAssignment(0);
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const bits = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr) << "Sec 20.6.2: 'bit_count = $bits(i)' RHS must be a SysFuncCall";
  EXPECT_EQ(bits->getName(), std::string_view("$bits"));
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "$bits(i) argument must be a RefObj naming 'i'";
  EXPECT_EQ(arg->getName(), std::string_view("i"));
  EXPECT_NE(arg->getActual(), nullptr) << "'i' inside $bits(i) must resolve to tb's 'i' variable";
}

// ---------------------------------------------------------------------------
// bit_count = $bits(l8)
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, BitsOfL8_IsSysFuncCallWithOneRefObjArgument) {
  const hldb::Assignment *const assign = getAssignment(1);
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const bits = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr) << "Sec 20.6.2: 'bit_count = $bits(l8)' RHS must be a SysFuncCall";
  EXPECT_EQ(bits->getName(), std::string_view("$bits"));
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "$bits(l8) argument must be a RefObj naming 'l8'";
  EXPECT_EQ(arg->getName(), std::string_view("l8"));
  EXPECT_NE(arg->getActual(), nullptr) << "'l8' inside $bits(l8) must resolve to tb's 'l8' variable";
}

// ---------------------------------------------------------------------------
// bit_count = $bits(real)  -- bare type name argument
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, BitsOfRealType_IsSysFuncCallWithOneArgument) {
  const hldb::Assignment *const assign = getAssignment(2);
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const bits = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr) << "Sec 20.6.2: 'bit_count = $bits(real)' RHS must be a SysFuncCall";
  EXPECT_EQ(bits->getName(), std::string_view("$bits"));
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  EXPECT_NE(bits->getArguments()->at(0), nullptr) << "$bits(real) must carry its bare type-name argument";
}

// ---------------------------------------------------------------------------
// type_name = $typename(s)
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, TypenameOfS_IsSysFuncCallWithOneRefObjArgument) {
  const hldb::Assignment *const assign = getAssignment(3);
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const tn = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(tn, nullptr) << "Sec 20.6.1: 'type_name = $typename(s)' RHS must be a SysFuncCall";
  EXPECT_EQ(tn->getName(), std::string_view("$typename"));
  ASSERT_NE(tn->getArguments(), nullptr);
  ASSERT_EQ(tn->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(tn->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "$typename(s) argument must be a RefObj naming 's'";
  EXPECT_EQ(arg->getName(), std::string_view("s"));
  EXPECT_NE(arg->getActual(), nullptr) << "'s' inside $typename(s) must resolve to tb's 's' variable";
}

// ---------------------------------------------------------------------------
// type_name = $typename(i)
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, TypenameOfI_IsSysFuncCallWithOneRefObjArgument) {
  const hldb::Assignment *const assign = getAssignment(4);
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const tn = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(tn, nullptr) << "Sec 20.6.1: 'type_name = $typename(i)' RHS must be a SysFuncCall";
  EXPECT_EQ(tn->getName(), std::string_view("$typename"));
  ASSERT_NE(tn->getArguments(), nullptr);
  ASSERT_EQ(tn->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(tn->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "$typename(i) argument must be a RefObj naming 'i'";
  EXPECT_EQ(arg->getName(), std::string_view("i"));
  EXPECT_NE(arg->getActual(), nullptr) << "'i' inside $typename(i) must resolve to tb's 'i' variable";
}

// ---------------------------------------------------------------------------
// type_name = $typename(logic)  -- bare type keyword argument
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, TypenameOfLogicType_IsSysFuncCallWithOneArgument) {
  const hldb::Assignment *const assign = getAssignment(5);
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const tn = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(tn, nullptr) << "Sec 20.6.1: 'type_name = $typename(logic)' RHS must be a SysFuncCall";
  EXPECT_EQ(tn->getName(), std::string_view("$typename"));
  ASSERT_NE(tn->getArguments(), nullptr);
  ASSERT_EQ(tn->getArguments()->size(), 1u);
  EXPECT_NE(tn->getArguments()->at(0), nullptr) << "$typename(logic) must carry its bare type-name argument";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(DatQuery01Test, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
