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

// Tests for 20.7--array-queries.sv (tags: 20.7)
//   module top();
//   logic [31:0] arr;
//   initial begin
//   	$display(":assert: (%d == 0)", $unpacked_dimensions(arr));
//   	$display(":assert: (%d == 1)", $dimensions(arr));
//   	$display(":assert: (%d == 1)", $increment(arr));
//   	$display(":assert: (%d == 0)", $right(arr));
//   	$display(":assert: (%d == 31)", $left(arr));
//   	$display(":assert: (%d == 0)", $low(arr));
//   	$display(":assert: (%d == 31)", $high(arr));
//   	$display(":assert: (%d == 32)", $size(arr));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.7, "Array query functions":
// "array_query_function ::= array_dimension_function ( array_identifier
// [ , dimension_expression ] ) | ..." -- "$left", "$right", "$low",
// "$high", "$increment" and "$size" take an array identifier and an
// optional dimension expression; "$dimensions" and "$unpacked_dimensions"
// take only the array identifier. Every call here uses the single-argument
// form with the module-level variable "arr".
//
// Checked:
//   - design has module "top" with exactly 1 Variable "arr": LogicTypespec,
//     vector, with exactly 1 range whose left/right bounds are Constant
//     unsigned int "31" / "0" (size 64, IntTypespec)
//   - module has exactly 1 process, an Initial whose body is a Begin
//     wrapping exactly 8 statements and no variables
//   - each statement is a SysTaskCall "$display" with exactly 2 arguments:
//     a Constant string format (size 144 for 18 characters, 152 for 19)
//     and a SysFuncCall naming the array query function, in source order
//   - each array query SysFuncCall has exactly 1 argument: a RefObj "arr"
//     resolving to the module Variable "arr"
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the values the queries return and that the
// ":assert:" comparisons hold) cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArrayQueriesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.7--array-queries.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getArrVariable() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("arr", mod->getVariables());
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::SysTaskCall *getDisplayCall(size_t index) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(index));
  }

  static const hldb::SysFuncCall *getQueryCall(size_t index) {
    const hldb::SysTaskCall *const display = getDisplayCall(index);
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }

  // Checks one range bound: a Constant unsigned int with the given decompiled
  // text, size 64, and an IntTypespec.
  static void checkRangeBound(const hldb::Constant *bound, std::string_view literal) {
    ASSERT_NE(bound, nullptr) << "range bound '" << literal << "' should be a Constant";
    EXPECT_EQ(bound->getConstType(), vpiUIntConst);
    EXPECT_EQ(bound->getSize(), 64);
    EXPECT_EQ(bound->getDecompile(), literal);
    const hldb::RefTypespec *const ref = bound->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr);
  }

  // Checks one '$display(<format>, <query>(arr))' statement: the format
  // Constant string, that the second argument is a SysFuncCall named
  // 'query', and that the call has exactly 1 argument, a RefObj "arr"
  // resolving to the module Variable "arr".
  static void checkQueryStmt(size_t index, std::string_view format, int size, std::string_view query) {
    const hldb::SysTaskCall *const call = getDisplayCall(index);
    ASSERT_NE(call, nullptr) << "statement " << index << " should be a SysTaskCall";
    EXPECT_EQ(call->getName(), "$display");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u);

    const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
    EXPECT_EQ(fmt->getConstType(), vpiStringConst);
    EXPECT_EQ(fmt->getSize(), size) << "format length * 8 bits";
    EXPECT_EQ(fmt->getValue(), format);
    const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
    ASSERT_NE(fmtRef, nullptr);
    EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

    const hldb::SysFuncCall *const queryCall = getQueryCall(index);
    ASSERT_NE(queryCall, nullptr) << "'" << query << "(arr)' should be a SysFuncCall";
    EXPECT_EQ(queryCall->getName(), query);
    ASSERT_NE(queryCall->getArguments(), nullptr);
    ASSERT_EQ(queryCall->getArguments()->size(), 1u) << "'" << query << "' is called with only the array identifier";

    const hldb::RefObj *const arg = any_cast<hldb::RefObj>(queryCall->getArguments()->at(0));
    ASSERT_NE(arg, nullptr) << "'arr' should be a RefObj";
    EXPECT_EQ(arg->getName(), "arr");
    EXPECT_EQ(arg->getActual<hldb::Variable>(), getArrVariable());
  }
};

// --- module / logic [31:0] arr; -----------------------------------------------

TEST_F(ArrayQueriesTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ArrayQueriesTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u) << "'logic [31:0] arr;' is the only variable declaration";
}

TEST_F(ArrayQueriesTest, ArrIsLogicVectorVariable) {
  const hldb::Variable *const arr = getArrVariable();
  ASSERT_NE(arr, nullptr) << "'arr' should be a Variable";
  EXPECT_EQ(arr->getName(), "arr");
  const hldb::RefTypespec *const ref = arr->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::LogicTypespec *const ts = ref->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ts, nullptr) << "'logic [31:0] arr' should have a LogicTypespec";
  EXPECT_TRUE(ts->getVector());
}

TEST_F(ArrayQueriesTest, ArrHasRange31To0) {
  const hldb::Variable *const arr = getArrVariable();
  ASSERT_NE(arr, nullptr);
  const hldb::RefTypespec *const ref = arr->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::LogicTypespec *const ts = ref->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u) << "'[31:0]' is a single packed range";

  const hldb::Range *const range = ts->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  checkRangeBound(range->getLeftExpr<hldb::Constant>(), "31");
  checkRangeBound(range->getRightExpr<hldb::Constant>(), "0");
}

// --- initial process ------------------------------------------------------------

TEST_F(ArrayQueriesTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ArrayQueriesTest, InitialBodyIsBeginWithEightStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 8u) << "eight '$display(...);' statements";
}

// --- $display(":assert: (...)", <query>(arr)); ----------------------------------

TEST_F(ArrayQueriesTest, UnpackedDimensionsCall) {
  checkQueryStmt(0, ":assert: (%d == 0)", 144, "$unpacked_dimensions");
}

TEST_F(ArrayQueriesTest, DimensionsCall) { checkQueryStmt(1, ":assert: (%d == 1)", 144, "$dimensions"); }

TEST_F(ArrayQueriesTest, IncrementCall) { checkQueryStmt(2, ":assert: (%d == 1)", 144, "$increment"); }

TEST_F(ArrayQueriesTest, RightCall) { checkQueryStmt(3, ":assert: (%d == 0)", 144, "$right"); }

TEST_F(ArrayQueriesTest, LeftCall) { checkQueryStmt(4, ":assert: (%d == 31)", 152, "$left"); }

TEST_F(ArrayQueriesTest, LowCall) { checkQueryStmt(5, ":assert: (%d == 0)", 144, "$low"); }

TEST_F(ArrayQueriesTest, HighCall) { checkQueryStmt(6, ":assert: (%d == 31)", 152, "$high"); }

TEST_F(ArrayQueriesTest, SizeCall) { checkQueryStmt(7, ":assert: (%d == 32)", 152, "$size"); }

// --- compiler diagnostics -----------------------------------------------------

TEST_F(ArrayQueriesTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
