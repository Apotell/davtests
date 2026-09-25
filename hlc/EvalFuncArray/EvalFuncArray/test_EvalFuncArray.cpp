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

// Tests for EvalFuncArray.hlc (tests/EvalFuncArray/dut.sv):
//   package earlgrey;
//     parameter int InfosPerBank = max_info_pages('{10, 1, 14, 18, 12});
//     parameter int InfoTypes = 5;
//     function automatic integer max_info_pages(int infos[InfoTypes]);
//       int current_max = 0;
//       for (int i = 0; i < InfoTypes; i++)
//         if (infos[i] > current_max) current_max = infos[i];
//       return current_max;
//     endfunction
//   endpackage
//
// This is the array-typed counterpart of EvalFunc: the constant function
// argument is not a scalar but an unpacked-array assignment pattern
// ('{...}), and the function's formal ("int infos[InfoTypes]") is itself an
// unpacked array of "InfoTypes" (5) elements. Per IEEE 1800-2023 Sec 13.4.3,
// "max_info_pages" still qualifies as a constant function: its single
// argument is an input, it contains no fork/hierarchical-reference/non-
// constant-call constructs, and it is invoked with an array literal (a
// constant expression, since every element -- 10, 1, 14, 18, 12 -- is a
// literal) inside a "parameter" initializer at package scope. This file has
// no :should_fail_because: tag and is expected to compile cleanly.
//
// Per IEEE 1800-2023 Sec 5.9/6.20.2 "int" is a synonym for a 32-bit signed
// "integer" typespec (IntTypespec in this object model, as used by
// 13.4.3--const-function).
//
// As with EvalFunc, "parameter InfosPerBank = max_info_pages(...)" is
// modeled via Scope::getParamAssigns() (a ParamAssign, not
// Parameter::getExpr()) -- confirmed by test_13.4.3_const-function.cpp and
// re-derived here from the real hldb headers, not from any .log dump.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_expr.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EvalFuncArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EvalFuncArray.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("earlgrey", m_design->getAllPackages());
  }

  static const hldb::Function *getFunc() {
    const hldb::Package *const pkg = getPkg();
    return (pkg == nullptr) ? nullptr : hldb::findByName<hldb::Function>("max_info_pages", pkg->getTaskFuncs());
  }

  static const hldb::ParamAssign *findParamAssign(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    return (pkg == nullptr) ? nullptr : hldb::findByName(name, pkg->getParamAssigns());
  }
};

TEST_F(EvalFuncArrayTest, PackageEarlgreyExists) { ASSERT_NE(getPkg(), nullptr); }

TEST_F(EvalFuncArrayTest, FunctionMaxInfoPagesExists) { ASSERT_NE(getFunc(), nullptr); }

// parameter int InfoTypes = 5;  -- plain scalar parameter, needed as the
// function's array-size bound.
TEST_F(EvalFuncArrayTest, ParamInfoTypesIsFive) {
  const hldb::ParamAssign *const pa = findParamAssign("InfoTypes");
  ASSERT_NE(pa, nullptr) << "'parameter int InfoTypes = 5;' should produce a ParamAssign binding";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(rhs, nullptr) << "'5' is a plain constant literal";
  EXPECT_EQ(rhs->getDecompile(), "5");
}

// function automatic integer max_info_pages(int infos[InfoTypes]);
// The formal "infos" is an unpacked array of int -- per 13.4.3 this is
// still a legal constant-function input argument (only output/inout/ref
// are disallowed, not array-typed inputs).
TEST_F(EvalFuncArrayTest, FunctionHasOneArrayTypedInputArgument) {
  const hldb::Function *const fn = getFunc();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const infos = fn->getIODecls()->at(0);
  ASSERT_NE(infos, nullptr);
  EXPECT_EQ(infos->getName(), "infos");
  EXPECT_EQ(infos->getDirection(), vpiInput)
      << "13.4.3 requires a constant function to have no output/inout/ref arguments";

  const hldb::RefTypespec *const rts = infos->getTypespec();
  ASSERT_NE(rts, nullptr) << "'infos' should carry a typespec describing its unpacked-array type";
  const hldb::ArrayTypespec *const arrTs = rts->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arrTs, nullptr) << "'int infos[InfoTypes]' should resolve to an ArrayTypespec, actual AnyType: "
                             << (rts->getActual() != nullptr ? static_cast<int>(rts->getActual()->getAnyType()) : -1);
  EXPECT_FALSE(arrTs->getPacked()) << "'int infos[InfoTypes]' is an unpacked array per IEEE 1800-2023 Sec 7.4";

  const hldb::RefTypespec *const elemRts = arrTs->getElemTypespec();
  ASSERT_NE(elemRts, nullptr);
  EXPECT_NE(elemRts->getActual<hldb::IntTypespec>(), nullptr)
      << "the array element type should resolve to IntTypespec ('int')";
}

// parameter int InfosPerBank = max_info_pages('{10, 1, 14, 18, 12});
TEST_F(EvalFuncArrayTest, ParamInfosPerBankCallsMaxInfoPagesWithFiveElementArray) {
  const hldb::ParamAssign *const pa = findParamAssign("InfosPerBank");
  ASSERT_NE(pa, nullptr) << "'parameter int InfosPerBank = max_info_pages(...);' should produce a ParamAssign";
  const hldb::Any *const rhs = pa->getRhs();
  ASSERT_NE(rhs, nullptr);

  if (const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(rhs)) {
    EXPECT_EQ(call->getName(), "max_info_pages");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::ArrayExpr *const arrArg = any_cast<hldb::ArrayExpr>(call->getArguments()->at(0));
    ASSERT_NE(arrArg, nullptr) << "'{10, 1, 14, 18, 12}' should be modeled as an ArrayExpr argument, actual "
                                   "AnyType: "
                                << static_cast<int>(call->getArguments()->at(0)->getAnyType());
    ASSERT_NE(arrArg->getExprs(), nullptr);
    ASSERT_EQ(arrArg->getExprs()->size(), 5u);
    static const char *const kExpected[5] = {"10", "1", "14", "18", "12"};
    for (size_t i = 0; i < 5; ++i) {
      const hldb::Constant *const elem = any_cast<hldb::Constant>(arrArg->getExprs()->at(i));
      ASSERT_NE(elem, nullptr) << "element " << i << " of the array literal should be a Constant";
      EXPECT_EQ(elem->getDecompile(), kExpected[i]) << "element " << i << " mismatch";
    }
  } else if (const hldb::Constant *const folded = any_cast<hldb::Constant>(rhs)) {
    // 13.4.3: a constant function call is evaluated at elaboration time; if HLC folds it into a Constant rather
    // than keeping the FuncCall + ArrayExpr, the folded value must still be the correct maximum: 18.
    EXPECT_EQ(folded->getDecompile(), "18")
        << "max_info_pages({10,1,14,18,12}) must fold to the maximum element, 18";
  } else {
    FAIL() << "ParamAssign RHS for 'InfosPerBank' is neither a FuncCall nor a Constant -- actual AnyType: "
           << static_cast<int>(rhs->getAnyType());
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
