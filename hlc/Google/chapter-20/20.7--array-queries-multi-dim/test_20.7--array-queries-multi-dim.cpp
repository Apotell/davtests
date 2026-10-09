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

// Tests for 20.7--array-queries-multi-dim.sv (tags: 20.7)
//   module top();
//   logic [31:0] arr [15:0];
//   initial begin
//   	$display(":assert: (%d == 2)", $dimensions(arr));
//   	$display(":assert: (%d == 1)", $increment(arr, 2));
//   	$display(":assert: (%d == 0)", $right(arr, 2));
//   	$display(":assert: (%d == 31)", $left(arr, 2));
//   	$display(":assert: (%d == 0)", $right(arr, 1));
//   	$display(":assert: (%d == 15)", $left(arr, 1));
//   	$display(":assert: (%d == 0)", $low(arr, 2));
//   	$display(":assert: (%d == 31)", $high(arr, 2));
//   	$display(":assert: (%d == 32)", $size(arr, 2));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.7, "Array query functions":
// "array_query_function ::= array_dimension_function ( array_identifier
// [ , dimension_expression ] ) | array_dimensions_function
// ( array_identifier ) | ..." -- "$left", "$right", "$low", "$high",
// "$increment" and "$size" take an optional dimension expression, while
// "$dimensions" takes only the array identifier. Here "arr" has one
// unpacked dimension [15:0] (dimension 1) and one packed dimension [31:0]
// (dimension 2).
//
// Checked:
//   - design has module "top" with exactly 1 Variable "arr" whose typespec
//     is a static ArrayTypespec: unpacked Range with Constant unsigned int
//     bounds "15" / "0", and an element typespec that is a vector
//     LogicTypespec with exactly 1 Range with bounds "31" / "0" (all bounds
//     size 64, IntTypespec)
//   - module has exactly 1 process, an Initial whose body is a Begin
//     wrapping exactly 9 statements and no variables
//   - each statement is a SysTaskCall "$display" with exactly 2 arguments:
//     a Constant string format (size 144 for 18 characters, 152 for 19)
//     and a SysFuncCall naming the array query function, in source order
//   - each array query SysFuncCall's first argument is a RefObj "arr"
//     resolving to the module Variable "arr"
//   - "$dimensions" has exactly 1 argument; every other call has exactly 2,
//     the second being a Constant unsigned int dimension "1" or "2"
//     (size 64, IntTypespec)
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
#include <hldb/array_typespec.h>
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

class ArrayQueriesMultiDimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.7--array-queries-multi-dim.hlc"}); }
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

  static const hldb::ArrayTypespec *getArrTypespec() {
    const hldb::Variable *const arr = getArrVariable();
    if (arr == nullptr) {
      return nullptr;
    }
    const hldb::RefTypespec *const ref = arr->getTypespec<hldb::RefTypespec>();
    if (ref == nullptr) {
      return nullptr;
    }
    return ref->getActual<hldb::ArrayTypespec>();
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

  // Checks one unsigned int Constant: the given decompiled text, size 64,
  // and an IntTypespec. Used for range bounds and dimension arguments.
  static void checkUIntConstant(const hldb::Constant *constant, std::string_view literal) {
    ASSERT_NE(constant, nullptr) << "'" << literal << "' should be a Constant";
    EXPECT_EQ(constant->getConstType(), vpiUIntConst);
    EXPECT_EQ(constant->getSize(), 64);
    EXPECT_EQ(constant->getDecompile(), literal);
    const hldb::RefTypespec *const ref = constant->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr);
  }

  // Checks one '$display(<format>, <query>(arr[, <dimension>]))' statement:
  // the format Constant string, that the second argument is a SysFuncCall
  // named 'query' whose first argument is a RefObj "arr" resolving to the
  // module Variable "arr", and -- when 'dimension' is non-empty -- that a
  // second argument is the unsigned int Constant 'dimension'.
  static void checkQueryStmt(size_t index, std::string_view format, int size, std::string_view query,
                             std::string_view dimension) {
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
    ASSERT_NE(queryCall, nullptr) << "'" << query << "(...)' should be a SysFuncCall";
    EXPECT_EQ(queryCall->getName(), query);
    ASSERT_NE(queryCall->getArguments(), nullptr);
    const size_t argCount = dimension.empty() ? 1u : 2u;
    ASSERT_EQ(queryCall->getArguments()->size(), argCount)
        << "'" << query << "' is called with the array identifier"
        << (dimension.empty() ? " only" : " and a dimension expression");

    const hldb::RefObj *const arg = any_cast<hldb::RefObj>(queryCall->getArguments()->at(0));
    ASSERT_NE(arg, nullptr) << "'arr' should be a RefObj";
    EXPECT_EQ(arg->getName(), "arr");
    EXPECT_EQ(arg->getActual<hldb::Variable>(), getArrVariable());

    if (!dimension.empty()) {
      checkUIntConstant(any_cast<hldb::Constant>(queryCall->getArguments()->at(1)), dimension);
    }
  }
};

// --- module / logic [31:0] arr [15:0]; ------------------------------------------

TEST_F(ArrayQueriesMultiDimTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ArrayQueriesMultiDimTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u) << "'logic [31:0] arr [15:0];' is the only variable declaration";
}

TEST_F(ArrayQueriesMultiDimTest, ArrIsStaticUnpackedArray15To0) {
  const hldb::Variable *const arr = getArrVariable();
  ASSERT_NE(arr, nullptr) << "'arr' should be a Variable";
  EXPECT_EQ(arr->getName(), "arr");

  const hldb::ArrayTypespec *const ts = getArrTypespec();
  ASSERT_NE(ts, nullptr) << "'arr [15:0]' should have an ArrayTypespec";
  EXPECT_EQ(ts->getArrayType(), vpiStaticArray) << "'[15:0]' is a fixed-size unpacked dimension";

  const hldb::Range *const range = ts->getRange();
  ASSERT_NE(range, nullptr);
  checkUIntConstant(range->getLeftExpr<hldb::Constant>(), "15");
  checkUIntConstant(range->getRightExpr<hldb::Constant>(), "0");
}

TEST_F(ArrayQueriesMultiDimTest, ArrElementIsLogicVector31To0) {
  const hldb::ArrayTypespec *const ts = getArrTypespec();
  ASSERT_NE(ts, nullptr);
  const hldb::RefTypespec *const elemRef = ts->getElemTypespec();
  ASSERT_NE(elemRef, nullptr);
  const hldb::LogicTypespec *const elem = elemRef->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr) << "the element type 'logic [31:0]' should be a LogicTypespec";
  EXPECT_TRUE(elem->getVector());
  ASSERT_NE(elem->getRanges(), nullptr);
  ASSERT_EQ(elem->getRanges()->size(), 1u) << "'[31:0]' is a single packed range";

  const hldb::Range *const range = elem->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  checkUIntConstant(range->getLeftExpr<hldb::Constant>(), "31");
  checkUIntConstant(range->getRightExpr<hldb::Constant>(), "0");
}

// --- initial process ------------------------------------------------------------

TEST_F(ArrayQueriesMultiDimTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ArrayQueriesMultiDimTest, InitialBodyIsBeginWithNineStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 9u) << "nine '$display(...);' statements";
}

// --- $display(":assert: (...)", <query>(arr[, <dimension>])); ------------------

TEST_F(ArrayQueriesMultiDimTest, DimensionsCall) { checkQueryStmt(0, ":assert: (%d == 2)", 144, "$dimensions", ""); }

TEST_F(ArrayQueriesMultiDimTest, IncrementDim2Call) { checkQueryStmt(1, ":assert: (%d == 1)", 144, "$increment", "2"); }

TEST_F(ArrayQueriesMultiDimTest, RightDim2Call) { checkQueryStmt(2, ":assert: (%d == 0)", 144, "$right", "2"); }

TEST_F(ArrayQueriesMultiDimTest, LeftDim2Call) { checkQueryStmt(3, ":assert: (%d == 31)", 152, "$left", "2"); }

TEST_F(ArrayQueriesMultiDimTest, RightDim1Call) { checkQueryStmt(4, ":assert: (%d == 0)", 144, "$right", "1"); }

TEST_F(ArrayQueriesMultiDimTest, LeftDim1Call) { checkQueryStmt(5, ":assert: (%d == 15)", 152, "$left", "1"); }

TEST_F(ArrayQueriesMultiDimTest, LowDim2Call) { checkQueryStmt(6, ":assert: (%d == 0)", 144, "$low", "2"); }

TEST_F(ArrayQueriesMultiDimTest, HighDim2Call) { checkQueryStmt(7, ":assert: (%d == 31)", 152, "$high", "2"); }

TEST_F(ArrayQueriesMultiDimTest, SizeDim2Call) { checkQueryStmt(8, ":assert: (%d == 32)", 152, "$size", "2"); }

// --- compiler diagnostics -----------------------------------------------------

TEST_F(ArrayQueriesMultiDimTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
