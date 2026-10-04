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

// Tests for dut.sv (tags: ForeachArray)
//   module dut;
//     int array[16];
//     initial begin
//       foreach(array[i])
//         array[i] = i;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 Sec 12.7.3 "The foreach loop
// construct", checked before any test code was written):
//   "foreach ( array_name [ loop_variables ] )" -- a single-dimension
//   unpacked array "int array[16]" iterated with exactly one loop variable
//   "i", which is a new automatic loop variable implicitly declared as
//   local to the foreach loop's own scope. Per 12.7.3: "The loop variables
//   ... are automatically... implicitly declared as a local variable of
//   int type" -- this is why ForeachStmt::getLoopVars() must hold a
//   Variable (with getIsIterator() true), not a RefObj to some
//   preexisting declaration.
//
// What is checked:
//   - module 'dut' exists with exactly 1 Initial process
//   - the initial's body is an explicit Begin (source has begin-end)
//     holding exactly 1 statement: the ForeachStmt
//   - the ForeachStmt's array_name resolves to a RefObj named "array"
//     (12.7.3: "array_name shall be ... the name of an array object")
//   - exactly 1 loop variable "i", implicitly declared (getIsIterator())
//   - the loop body ("array[i] = i;") is directly an Assignment (no
//     begin-end wraps a single foreach body statement): LHS is a RefObj
//     whose flattened name is "array[i]" and RHS is a RefObj "i"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForeachArray.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() {
    return hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  }

  static const hldb::Initial *getInitial() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *dut->getProcesses()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(p)) return init;
    }
    return nullptr;
  }

  static const hldb::ForeachStmt *getForeach() {
    const hldb::Initial *const init = getInitial();
    if (init == nullptr) return nullptr;
    const hldb::Begin *const body = init->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(0));
  }
};

// ---------------------------------------------------------------------------
// Module / process existence
// ---------------------------------------------------------------------------

TEST_F(ForeachArrayTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(ForeachArrayTest, InitialExists) { EXPECT_NE(getInitial(), nullptr); }

TEST_F(ForeachArrayTest, InitialBodyIsExplicitBeginWithOneStmt) {
  const hldb::Initial *const init = getInitial();
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "initial body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u);
}

// ---------------------------------------------------------------------------
// 12.7.3: foreach(array[i]) -- array_name and loop_variables
// ---------------------------------------------------------------------------

TEST_F(ForeachArrayTest, ForeachStmtExists) { EXPECT_NE(getForeach(), nullptr); }

TEST_F(ForeachArrayTest, ArrayNameIsRefObjArray) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getVariable(), nullptr) << "12.7.3: array_name must be present";
  EXPECT_EQ(fe->getVariable()->getName(), std::string_view{"array"});
}

TEST_F(ForeachArrayTest, OneImplicitLoopVariableI) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr) << "12.7.3: 'foreach(array[i])' declares exactly one loop variable";
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const i = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  ASSERT_NE(i, nullptr) << "12.7.3: loop variable 'i' must be an implicitly declared Variable";
  EXPECT_EQ(i->getName(), std::string_view{"i"});
  EXPECT_TRUE(i->getIsIterator()) << "12.7.3: loop variables are automatically/implicitly declared";
}

// ---------------------------------------------------------------------------
// Loop body: "array[i] = i;"
// ---------------------------------------------------------------------------

TEST_F(ForeachArrayTest, BodyIsDirectlyAssignArrayIEqualsI) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::Assignment *const assign = fe->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'array[i] = i;' has no begin-end, so it is directly the ForeachStmt's body";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"array[i]"});
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "RHS 'i' should be a RefObj";
  EXPECT_EQ(rhs->getName(), std::string_view{"i"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
