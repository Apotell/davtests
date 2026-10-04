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

// Source under test: tests/Google/chapter-18/18.12--randomization-of-scope-variables_0.sv
//
//   class a;
//       function int do_randomize();
//           int x, success;
//           success = std::randomize(x);
//           return success;
//       endfunction
//   endclass
//
// No module, no UVM import, and (unlike the sibling
// 18.12.1--adding-constraints-to-scope-variables_0.sv) no formal argument
// and no in-line "with {...}" constraint block -- just a bare
// "std::randomize(x);" call. This tool is a compiler/elaborator, not a
// simulator, so this test only checks the static declarations.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide). Every point below was
// confirmed by actually compiling and running this test's own
// SetUpTestSuite(), not by reading the existing .log:
//   - Sec 18.12 ("Randomization of scope variables"): "std::randomize(x)"
//     is the built-in *global* randomize function (scope-qualified via
//     "std::"), able to randomize the plain local variable "x" even
//     though it is not a class rand/randc property and carries no
//     constraint at all here.
//   - "do_randomize" is a fully-bodied method, so it is modeled as an
//     hldb::Function found via ClassDefn::getMethods() (not
//     ClassDefn::getTaskFuncDecls(), which is for prototype-only
//     declarations; ClassDefn extends Scope directly, not Instance, so
//     it has no Instance::getTaskFuncs()-style accessor either).
//   - Sec 6.11: the function's return type ("int") resolves to a signed
//     IntTypespec via Function::getReturn().
//   - The function takes no formal arguments: Function::getIODecls() is
//     null/empty.
//   - The local variables "int x, success;" are declared in the function
//     body. An earlier version of this test wrongly assumed they would
//     be found via Function::getVariables() (which is null) -- confirmed
//     by actually running this test: a multi-statement function body
//     with no explicit begin/end in the source is still wrapped in an
//     implicit hldb::Begin behind Function::getStmt(), and that Begin --
//     not the Function object itself -- holds the body's local variable
//     declarations.
//
// Not checked:
//   - The "success = std::randomize(x);" assignment statement itself and
//     its call-site shape (only the enclosing Begin's own declarations
//     were walked, not its individual statements).
//   - Any actual runtime randomize()/return-value behavior -- unrunnable
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
#include <hldb/int_typespec.h>
#include <hldb/begin.h>

namespace hlc {
class RandomizationOfScopeVariables0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.12--randomization-of-scope-variables_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RandomizationOfScopeVariables0Test, MethodDoRandomizeExistsWithIntReturnAndNoArguments) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_randomize", a->getMethods());
  ASSERT_NE(fn, nullptr);

  const hldb::IntTypespec *const returnTs = hldb::getActual<hldb::IntTypespec>(fn->getReturn());
  ASSERT_NE(returnTs, nullptr) << "'function int do_randomize()' should return a signed IntTypespec";
  EXPECT_TRUE(returnTs->getSigned());

  EXPECT_EQ(fn->getIODecls(), nullptr) << "'do_randomize()' takes no formal arguments";
}

TEST_F(RandomizationOfScopeVariables0Test, LocalVariablesXAndSuccessExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_randomize", a->getMethods());
  ASSERT_NE(fn, nullptr);

  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a multi-statement function body should be wrapped in an implicit Begin";
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", body->getVariables());
  const hldb::Variable *const success = hldb::findByName<hldb::Variable>("success", body->getVariables());
  ASSERT_NE(x, nullptr) << "'int x, success;' declares both locals in the function body's implicit Begin scope";
  ASSERT_NE(success, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
