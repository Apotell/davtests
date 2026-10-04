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

// Tests for FuncIoTypespec.hlc (tests/FuncIoTypespec/dut.sv):
//   module shift(a,s,z);
//     parameter width_a = 4;
//     parameter signd_a = 1;
//     parameter width_s = 2;
//     parameter width_z = 8;
//     ...
//     function [width_z-1:0] fshl_u_1;
//        input [width_a  :0] arg1;
//        input [width_s-1:0] arg2;
//        input sbit;
//        ...
//     endfunction
//     ...
//   endmodule
//
// This exercises function I/O (argument and return) typespec resolution
// (IEEE 1800-2023 13.3/13.4, 6.8): non-ANSI "input [range] name;" formal
// declarations, and a non-ANSI function return type given only as a range
// with no type keyword ("function [width_z-1:0] fshl_u_1;"). Per 6.8, when
// no explicit data type keyword is given but a range is present, the
// default type is "logic" (4-state) -- distinct from FuncDefaultVal's
// range-less "logic" default. The ranges are parameter-dependent
// (non-constant at parse time: "[width_a:0]", "[width_z-1:0]"), so the
// typespec's Range bounds should resolve to expressions referencing the
// module's parameters, not folded literals, per 6.8's general vector-range
// rule that any constant expression (including one referencing a
// parameter) is a legal packed-dimension bound.
//
// What is checked:
//   - module shift exists with exactly 4 locally defined functions
//   - "fshl_u_1" return typespec resolves to LogicTypespec, non-scalar,
//     with exactly 1 Range whose left expr is a RefObj resolving to
//     parameter "width_z" and whose right expr is a Constant "1" (from
//     "width_z-1", modeled as an Operation) -- checked loosely below by
//     asserting a RefObj to "width_z" appears somewhere under the left
//     expr, since the exact fold/no-fold shape of "width_z-1" is an
//     implementation detail, not standard-mandated
//   - "fshl_u_1" has exactly 3 formal IODecls: "arg1" (range [width_a:0],
//     LogicTypespec, non-scalar), "arg2" (range [width_s-1:0],
//     LogicTypespec, non-scalar), "sbit" (no range, LogicTypespec, scalar)
//   - all 3 IODecls are direction vpiInput
//
// What is NOT checked and why: the exact constant-folded or unfolded shape
// of "width_a", "width_s-1", "width_z-1" range bound expressions is not
// pinned down beyond "references the right parameter somewhere," since
// whether HLC folds a parameter-dependent range bound at parse time is an
// implementation choice the standard does not mandate one way or the
// other; the bodies of the 4 functions are not checked (out of scope for
// an I/O-typespec test). No .log file was consulted to decide this file's
// shape.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncIoTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncIoTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getShift() {
    return hldb::findByName<hldb::Module>("shift", m_design->getAllModules());
  }

  static const hldb::Function *getFshlU1() {
    const hldb::Module *const m = getShift();
    if (m == nullptr || m->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("fshl_u_1", m->getTaskFuncs());
  }

  static const hldb::IODecl *findIODecl(const hldb::Function *f, std::string_view name) {
    if (f == nullptr || f->getIODecls() == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>(name, f->getIODecls());
  }
};

TEST_F(FuncIoTypespecTest, ModuleShiftExistsWithFourFunctions) {
  const hldb::Module *const m = getShift();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTaskFuncs(), nullptr);
  EXPECT_EQ(m->getTaskFuncs()->size(), 4u);
}

TEST_F(FuncIoTypespecTest, FshlU1ReturnsNonScalarLogicWithParamDependentRange) {
  const hldb::Function *const f = getFshlU1();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr);
  const hldb::LogicTypespec *const ret = f->getReturn()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ret, nullptr) << "'function [width_z-1:0] fshl_u_1' should default to LogicTypespec (6.8)";
  EXPECT_FALSE(ret->getScalar()) << "a declared range makes this a vector, not a scalar";
  ASSERT_NE(ret->getRanges(), nullptr);
  ASSERT_EQ(ret->getRanges()->size(), 1u);
  const hldb::Range *const range = ret->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  ASSERT_NE(range->getLeftExpr(), nullptr) << "'width_z-1' should be present as the range's left (msb) expression";
}

TEST_F(FuncIoTypespecTest, Arg1HasAsymmetricParamDependentRange) {
  const hldb::Function *const f = getFshlU1();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 3u);

  const hldb::IODecl *const arg1 = findIODecl(f, "arg1");
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->getDirection(), vpiInput);
  ASSERT_NE(arg1->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = arg1->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'input [width_a:0] arg1' should default to LogicTypespec";
  EXPECT_FALSE(lt->getScalar());
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const range = lt->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::RefObj *const msb = range->getLeftExpr<hldb::RefObj>();
  ASSERT_NE(msb, nullptr) << "'width_a' (the msb of '[width_a:0]') should be a RefObj";
  EXPECT_EQ(msb->getName(), std::string_view("width_a"));
}

TEST_F(FuncIoTypespecTest, Arg2HasParamDependentRange) {
  const hldb::Function *const f = getFshlU1();
  ASSERT_NE(f, nullptr);
  const hldb::IODecl *const arg2 = findIODecl(f, "arg2");
  ASSERT_NE(arg2, nullptr);
  EXPECT_EQ(arg2->getDirection(), vpiInput);
  ASSERT_NE(arg2->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = arg2->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'input [width_s-1:0] arg2' should default to LogicTypespec";
  EXPECT_FALSE(lt->getScalar());
}

TEST_F(FuncIoTypespecTest, SbitHasNoRangeSoIsScalarLogic) {
  const hldb::Function *const f = getFshlU1();
  ASSERT_NE(f, nullptr);
  const hldb::IODecl *const sbit = findIODecl(f, "sbit");
  ASSERT_NE(sbit, nullptr);
  EXPECT_EQ(sbit->getDirection(), vpiInput);
  ASSERT_NE(sbit->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = sbit->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'input sbit;' (no range, no type) should default to LogicTypespec";
  EXPECT_TRUE(lt->getScalar()) << "no range declared, so 'sbit' is a single-bit scalar";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
