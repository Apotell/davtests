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

// Source under test: tests/Google/chapter-18/18.13.1--urandom_0.sv
//
//   class a;
//       function int unsigned do_urandom();
//           int unsigned x;
//           x = $urandom();
//           return x;
//       endfunction
//   endclass
//
// No module, no UVM import -- just a class with one method. This tool is
// a compiler/elaborator, not a simulator, so this test only checks the
// static declarations, not what "$urandom()" actually returns at
// runtime.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Sec 18.13.1 ("$urandom"): "$urandom()" with no argument is the
//     zero-argument form of the system function; it is a call-site
//     construct inside the function body, not a declaration, so its own
//     object shape is not asserted here (see "Not checked").
//   - "do_urandom" is a fully-bodied method, so it is modeled as an
//     hldb::Function found via ClassDefn::getMethods() -- not
//     ClassDefn::getTaskFuncDecls() (that collection is for
//     prototype-only declarations) and not Instance::getTaskFuncs()
//     (ClassDefn extends Scope directly, not Instance, so it has no such
//     accessor) -- same modeling already confirmed for the sibling
//     "randomization of scope variables" tests in this chapter.
//   - Sec 6.11 ("int unsigned" is the 2-state 32-bit integer type with
//     the "unsigned" qualifier): the function's return type resolves to
//     an IntTypespec via Function::getReturn(), with getSigned() false.
//   - The function takes no formal arguments: Function::getIODecls() is
//     null/empty.
//   - The local variable "int unsigned x;" is declared in the function
//     body. Confirmed by actually compiling and running this test (an
//     earlier version of this test wrongly assumed it would be found via
//     Function::getVariables(), which is null): a multi-statement
//     function body with no explicit begin/end in the source is still
//     wrapped in an implicit hldb::Begin behind Function::getStmt(), and
//     that Begin -- not the Function object itself -- holds the body's
//     local variable declarations. "x" resolves to an unsigned
//     IntTypespec.
//   - The whole file is legal SV; zero compiler diagnostics are expected.
//
// Not checked:
//   - The "x = $urandom();" assignment statement itself and its
//     SysFuncCall("$urandom") RHS (only the enclosing Begin's own
//     declarations were walked, not its individual statements).
//   - Any actual runtime $urandom()/return-value behavior -- unrunnable
//     by a compiler/elaborator.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/class_defn.h>
#include <hldb/function.h>
#include <hldb/variable.h>
#include <hldb/begin.h>

#include <iostream>
#include <hldb/int_typespec.h>

namespace hlc {
class Urandom0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.13.1--urandom_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Urandom0Test, MethodDoUrandomExistsWithUnsignedIntReturnAndNoArguments) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_urandom", a->getMethods());
  ASSERT_NE(fn, nullptr);

  const hldb::IntTypespec *const returnTs = hldb::getActual<hldb::IntTypespec>(fn->getReturn());
  ASSERT_NE(returnTs, nullptr) << "'function int unsigned do_urandom()' should return an IntTypespec";
  EXPECT_FALSE(returnTs->getSigned()) << "Sec 6.11: 'int unsigned' is unsigned";

  EXPECT_EQ(fn->getIODecls(), nullptr) << "'do_urandom()' takes no formal arguments";
}

TEST_F(Urandom0Test, LocalVariableXIsUnsignedInt) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_urandom", a->getMethods());
  ASSERT_NE(fn, nullptr);

  // Confirmed by actually compiling and running this test: a function
  // body with more than one statement (no explicit begin/end in the
  // source) is still wrapped in an implicit hldb::Begin behind
  // Function::getStmt(), and that Begin -- not the Function object
  // itself -- is what holds the body's local variable declarations.
  // Function::getVariables() is null here.
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a multi-statement function body should be wrapped in an implicit Begin";
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", body->getVariables());
  ASSERT_NE(x, nullptr) << "'int unsigned x;' declares 'x' in the function body's implicit Begin scope";

  const hldb::IntTypespec *const xTs = hldb::getTypespec<hldb::IntTypespec>(x);
  ASSERT_NE(xTs, nullptr);
  EXPECT_FALSE(xTs->getSigned());
}

TEST_F(Urandom0Test, CompilerReportsZeroErrors) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
