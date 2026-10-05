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

// Tests for dut.sv (tags: LateBindingFuncArg)
//   `default_nettype none
//
//   module top;
//       function integer doFoo(input integer depth);
//           doFoo = $clog2($bits(depth));
//       endfunction
//   endmodule // top
//
// What is checked (IEEE 1800-2023):
//   - 22.8: `default_nettype none is in effect for module top, so its
//     default net type is vpiNone
//   - top contains exactly one function "doFoo" (13.4) returning "integer"
//     (6.11: integer is a signed 32-bit 4-state type); it is a static
//     function (13.4.2: functions in modules default to static lifetime)
//     with public visibility (37.41 detail 4: not a class member)
//   - 37.41: vpiFuncType of a function returning integer is vpiIntFunc
//   - doFoo has a single formal "depth" with direction input (13.4) and
//     type integer
//   - the body is one blocking assignment (10.4.1) whose LHS "doFoo" is the
//     implicit variable named after the function that captures its return
//     value (13.4.1); the RHS is $clog2(...) (20.8.1) whose single argument
//     is $bits(depth) (20.6.2), and "depth" binds to the formal argument
//     (the "late binding" this test is named after -- the argument is
//     referenced inside a nested system-function call)
//   - with `default_nettype none, every identifier used here is declared,
//     so none of them fails to bind (22.8)
//
// What is NOT checked and why:
//   - the evaluated value $clog2($bits(depth)) == $clog2(32) == 5: requires
//     constant evaluation/elaboration not performed by this .hlc.
//   - 37.41 detail 3 ("vpiReturn shall always return a var object"): HLDB
//     models Function::getReturn() as a RefTypespec by design of its object
//     model, so the typespec is checked instead.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/integer_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class LateBindingFuncArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LateBindingFuncArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getDoFoo() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("doFoo", top->getTaskFuncs());
  }

  static const hldb::Assignment *getBody() {
    const hldb::Function *const f = getDoFoo();
    if (f == nullptr) return nullptr;
    return f->getStmt<hldb::Assignment>();
  }
};

TEST_F(LateBindingFuncArgTest, TopDefaultNetTypeIsNone) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefNetType(), vpiNone) << "22.8: `default_nettype none";
}

TEST_F(LateBindingFuncArgTest, TopHasSingleFunctionDoFoo) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const f = getDoFoo();
  ASSERT_NE(f, nullptr);
  EXPECT_FALSE(f->getAutomatic()) << "13.4.2: module functions are static by default";
  EXPECT_EQ(f->getVisibility(), vpiPublicVis) << "37.41 detail 4";
}

TEST_F(LateBindingFuncArgTest, DoFooReturnsSignedInteger) {
  const hldb::Function *const f = getDoFoo();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr);
  const hldb::IntegerTypespec *const it = f->getReturn()->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(it, nullptr) << "return type is 'integer'";
  EXPECT_TRUE(it->getSigned()) << "6.11: integer is signed";
}

TEST_F(LateBindingFuncArgTest, DoFooFuncTypeIsIntFunc) {
  const hldb::Function *const f = getDoFoo();
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->getFuncType(), vpiIntFunc) << "37.41: a function returning integer has vpiFuncType vpiIntFunc";
}

TEST_F(LateBindingFuncArgTest, DepthIsInputIntegerFormal) {
  const hldb::Function *const f = getDoFoo();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = f->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "depth");
  EXPECT_EQ(io->getDirection(), vpiInput);
  ASSERT_NE(io->getTypespec(), nullptr);
  EXPECT_NE(io->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr) << "depth is declared 'integer'";
}

TEST_F(LateBindingFuncArgTest, BodyAssignsToFunctionReturnVariable) {
  const hldb::Assignment *const a = getBody();
  ASSERT_NE(a, nullptr) << "the function body is a single assignment";
  EXPECT_TRUE(a->getBlocking());
  const hldb::RefObj *const lhs = a->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "doFoo");
  EXPECT_NE(lhs->getActual(), nullptr) << "13.4.1: the function name is an implicit variable for the return value";
}

TEST_F(LateBindingFuncArgTest, RhsIsClog2OfBitsOfDepth) {
  const hldb::Assignment *const a = getBody();
  ASSERT_NE(a, nullptr);
  const hldb::SysFuncCall *const clog2 = a->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(clog2, nullptr);
  EXPECT_EQ(clog2->getName(), "$clog2");
  ASSERT_NE(clog2->getArguments(), nullptr);
  ASSERT_EQ(clog2->getArguments()->size(), 1u);

  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(clog2->getArguments()->at(0));
  ASSERT_NE(bits, nullptr);
  EXPECT_EQ(bits->getName(), "$bits");
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::RefObj *const depth = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(depth, nullptr);
  EXPECT_EQ(depth->getName(), "depth");
  ASSERT_NE(depth->getActual(), nullptr) << "'depth' must bind to the function's formal argument";
  const hldb::Function *const f = getDoFoo();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  EXPECT_EQ(depth->getActual(), f->getIODecls()->at(0));
}

TEST_F(LateBindingFuncArgTest, NoBindingFailures) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "depth"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "doFoo"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
