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

// Source under test: tests/Google/chapter-18/18.13.1--urandom_2.sv
//
//   class a;
//       function int unsigned do_urandom(int seed);
//           int unsigned x;
//           x = $urandom(seed);
//           return x;
//       endfunction
//   endclass
//
// No module, no UVM import. Unlike the sibling 18.13.1--urandom_0.sv,
// this method takes a "seed" argument and calls the one-argument form
// "$urandom(seed)" (Sec 18.13.1: the optional seed argument reseeds the
// RNG deterministically). This tool is a compiler/elaborator, not a
// simulator, so this test only checks the static declarations.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - "do_urandom" is a fully-bodied method, so it is modeled as an
//     hldb::Function found via ClassDefn::getMethods() (same modeling
//     already confirmed for the sibling "_0" file and for this chapter's
//     other class-method tests).
//   - Sec 6.11: the function's return type ("int unsigned") resolves to
//     an unsigned IntTypespec via Function::getReturn().
//   - Sec 13.3 (function arguments): the formal argument "seed" is an
//     IODecl with direction vpiInput, resolving (via
//     hldb::getTypespec<IntTypespec>()) to a *signed* IntTypespec (plain
//     "int", no "unsigned" qualifier on the argument itself -- distinct
//     from the unsigned return type and unsigned local "x").
//   - The local variable "int unsigned x;" is declared in the function
//     body. Confirmed by actually compiling and running this test (an
//     earlier version of this test wrongly assumed it would be found via
//     Function::getVariables(), which is null): a multi-statement
//     function body with no explicit begin/end in the source is wrapped
//     in an implicit hldb::Begin behind Function::getStmt(), and that
//     Begin -- not the Function object itself -- holds the body's local
//     variable declarations. "x" resolves to an unsigned IntTypespec.
//   - The whole file is legal SV; zero compiler diagnostics are expected.
//
// Not checked:
//   - The "x = $urandom(seed);" assignment statement itself and its
//     SysFuncCall("$urandom") RHS with its one argument (only the
//     enclosing Begin's own declarations were walked, not its individual
//     statements).
//   - Any actual runtime $urandom(seed)/return-value/reseeding behavior
//     -- unrunnable by a compiler/elaborator.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/class_defn.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/variable.h>
#include <hldb/int_typespec.h>
#include <hldb/begin.h>
#include <hldb/vpi_user.h>

namespace hlc {
class Urandom2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.13.1--urandom_2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Urandom2Test, MethodDoUrandomExistsWithUnsignedIntReturnAndSignedIntArgSeed) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_urandom", a->getMethods());
  ASSERT_NE(fn, nullptr);

  const hldb::IntTypespec *const returnTs = hldb::getActual<hldb::IntTypespec>(fn->getReturn());
  ASSERT_NE(returnTs, nullptr) << "'function int unsigned do_urandom(...)' should return an IntTypespec";
  EXPECT_FALSE(returnTs->getSigned()) << "Sec 6.11: 'int unsigned' is unsigned";

  const hldb::IODeclCollection *const ioDecls = fn->getIODecls();
  ASSERT_NE(ioDecls, nullptr);
  const hldb::IODecl *const seed = hldb::findByName<hldb::IODecl>("seed", ioDecls);
  ASSERT_NE(seed, nullptr) << "'int seed' should be a formal argument";
  EXPECT_EQ(seed->getDirection(), vpiInput);

  const hldb::IntTypespec *const seedTs = hldb::getTypespec<hldb::IntTypespec>(seed);
  ASSERT_NE(seedTs, nullptr);
  EXPECT_TRUE(seedTs->getSigned()) << "'int seed' (no 'unsigned' qualifier) is signed";
}

TEST_F(Urandom2Test, LocalVariableXIsUnsignedInt) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_urandom", a->getMethods());
  ASSERT_NE(fn, nullptr);

  // Confirmed by actually compiling and running this test: a
  // multi-statement function body with no explicit begin/end in the
  // source is wrapped in an implicit hldb::Begin behind
  // Function::getStmt(), and that Begin -- not the Function object
  // itself -- holds the body's local variable declarations.
  // Function::getVariables() is null here.
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a multi-statement function body should be wrapped in an implicit Begin";
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", body->getVariables());
  ASSERT_NE(x, nullptr) << "'int unsigned x;' declares 'x' in the function body's implicit Begin scope";

  const hldb::IntTypespec *const xTs = hldb::getTypespec<hldb::IntTypespec>(x);
  ASSERT_NE(xTs, nullptr);
  EXPECT_FALSE(xTs->getSigned());
}

TEST_F(Urandom2Test, CompilerReportsZeroErrors) {
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
