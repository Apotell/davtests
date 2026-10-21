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

// Validates the HLDB model built for tests/ParamOverload3/dut.sv:
//
//   package  fpnew_pkg;
//     localparam int unsigned NUM_FP_FORMATS = 5;
//     localparam int unsigned FP_FORMAT_BITS = $clog2(NUM_FP_FORMATS);
//    typedef struct packed {
//       int unsigned exp_bits;
//       int unsigned man_bits;
//     } fp_encoding_t;
//     typedef enum logic [FP_FORMAT_BITS-1:0] {
//       FP32    = 'd0,
//       FP64    = 'd1,
//       FP16    = 'd2,
//       FP8     = 'd3,
//       FP16ALT = 'd4
//       // add new formats here
//     } fp_format_e;
//     localparam fp_encoding_t [0:NUM_FP_FORMATS-1] FP_ENCODINGS  = '{
//       '{8,  23}, '{11, 52}, '{5,  10}, '{5,  2}, '{8,  7}
//     };
//     localparam fpu_features_t RV64D_Xsflt = '{
//       Width:         64,
//       EnableVectors: 1'b1,
//       EnableNanBox:  1'b1,
//       FpFmtMask:     5'b11111,
//       IntFmtMask:    4'b1111
//     };
//     function automatic int unsigned fp_width(fp_format_e fmt);
//       return FP_ENCODINGS[fmt].exp_bits + FP_ENCODINGS[fmt].man_bits + 1;
//     endfunction
//   endpackage
//
//   module top();
//      localparam int unsigned NUM_FORMATS  = fpnew_pkg::NUM_FP_FORMATS;
//      parameter fpnew_pkg::fpu_features_t       Features       = fpnew_pkg::RV64D_Xsflt;
//      parameter WIDTH = Features.Width;
//     for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_nanbox_check
//       localparam int unsigned FP_WIDTH = fpnew_pkg::fp_width(fpnew_pkg::fp_format_e'(fmt));
//       if (Features.EnableNanBox && (FP_WIDTH < WIDTH)) begin : check
//         for (genvar op = 0; op < int'(NUM_OPERANDS); op++) begin : operands
//           assign is_boxed[fmt][op] = (!vectorial_op_i)
//                                      ? operands_i[op][WIDTH-1:FP_WIDTH] == '1
//                                      : 1'b1;
//         end
//       end else begin : no_check
//         assign is_boxed[fmt] = '1;
//       end
//     end
//   endmodule
//
// (The comments inside FP_ENCODINGS are elided above.)
//
// The point of the fixture is a package of constants, a packed structure, an
// enumeration whose width is computed with $clog2, an array of structures and
// a constant function (13.4.3), all used by a loop generate in a module. The
// fixture is illegal: the type fpu_features_t is declared nowhere, yet both
// the package's RV64D_Xsflt and top's Features are declared with it. 6.18:
// "The declaration of a user-defined data type shall precede any reference to
// its type_identifier." The regression this file exists to catch is HLC
// accepting the undeclared type silently, or losing the legal parts of the
// package around it.
//
// Values: NUM_FP_FORMATS = 5 and FP_FORMAT_BITS = $clog2(5) = 3 (20.8.1), so
// fp_format_e's base type is logic [2:0].
//
// What is checked, and why:
//   Package fpnew_pkg
//     - exactly 4 parameters, all local: they are declared 'localparam'
//       (6.20.4) in a package (6.20.1)
//     - NUM_FP_FORMATS is int unsigned and holds 5
//     - FP_FORMAT_BITS is int unsigned, and its unreduced value is the
//       system function call $clog2 (20.8.1) with one argument, the RefObj
//       NUM_FP_FORMATS bound to the package's Parameter
//     - fp_encoding_t is a packed structure (7.2.1) of exactly 2 members,
//       exp_bits and man_bits, both int unsigned
//     - fp_format_e is an enumeration (6.19) whose base type is logic
//       [FP_FORMAT_BITS-1:0], with exactly 5 constants, FP32 to FP16ALT in
//       order, valued 'd0 to 'd4
//     - FP_ENCODINGS's value is a pattern of 5 nested positional patterns
//       (10.9.1, 10.9.2) holding {8, 23}, {11, 52}, {5, 10}, {5, 2} and
//       {8, 7}
//     - fp_width is an automatic function (13.4.2) returning int unsigned
//       with exactly 1 argument, fmt, an input of type fp_format_e
//   Module top
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - NUM_FORMATS is a localparam whose unreduced value is the scoped name
//       fpnew_pkg::NUM_FP_FORMATS, bound to the package's Parameter (26.3)
//     - the only generate construct is a loop (27.4) whose condition is
//       vpiLtOp over the RefObj fmt and the cast int'(NUM_FORMATS)
//       (6.24.1), and whose block is named gen_nanbox_check
//   Diagnostics
//     - the undeclared type fpu_features_t is reported as an error (6.18)
//     - there is no fatal and no syntax diagnostic
//
// Reduction and elaboration: none is checked. The design is illegal, so the
// standard gives no elaborated value for it.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port. TopHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (undeclared type only warned), not a defect in this
// test: HLC reports both uses of the undeclared fpu_features_t with a
// "Failed to bind" warning only, although 6.18 says "The declaration of a
// user-defined data type shall precede any reference to its
// type_identifier". UndeclaredFpuFeaturesTIsAnError is expected to fail
// until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The names NUM_OPERANDS, is_boxed, vectorial_op_i and operands_i, which
//     are also declared nowhere. They appear only inside the 'if' and 'else'
//     blocks, and which of those is instantiated depends on Features, whose
//     type is the undeclared fpu_features_t, so the standard does not decide
//     which of them must be resolved.
//   - Which error identifier HLC uses is a tool convention; any
//     error-severity diagnostic naming fpu_features_t is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/function.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <utility>

namespace hlc {

class ParamOverload3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamOverload3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of error severity names 'symbol'. Which error
  // identifier HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      if ((entry.second.m_severity == ErrorDefinition::ERROR) && (findError(entry.first, symbol) != nullptr)) {
        return true;
      }
    }
    return false;
  }

  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("fpnew_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getTypedef(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, pkg->getTypedefs());
  }

  static const hldb::Function *getFpWidth() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("fp_width", pkg->getTaskFuncs());
  }

  static const hldb::GenFor *getLoop() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenStmts() == nullptr || top->getGenStmts()->size() != 1u) return nullptr;
    return any_cast<hldb::GenFor>(top->getGenStmts()->at(0));
  }

  // Whether 'rts' names the package typedef 'name'.
  static bool namesTypedef(const hldb::RefTypespec *rts, std::string_view name) {
    const hldb::Typedef *const td = getTypedef(name);
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  // Verifies 'rts' is an int unsigned.
  static void ExpectIntUnsigned(const hldb::RefTypespec *rts, std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no type";
    const hldb::IntTypespec *const it = rts->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "6.11: " << what << " is an int";
    EXPECT_FALSE(it->getSigned()) << "6.11.3: " << what << " is 'int unsigned'";
  }
};

// ---------------------------------------------------------------------------
// package fpnew_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamOverload3Test, PackageDeclaresFourLocalParameters) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'fpnew_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 4u);
  for (std::string_view name : {"NUM_FP_FORMATS", "FP_FORMAT_BITS", "FP_ENCODINGS", "RV64D_Xsflt"}) {
    const hldb::Parameter *const p = getParam(pkg, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << name << "' is declared 'localparam'";
  }
}

TEST_F(ParamOverload3Test, NumFpFormatsIsFive) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = getParam(pkg, "NUM_FP_FORMATS");
  ASSERT_NE(p, nullptr);
  ExpectIntUnsigned(p->getTypespec(), "NUM_FP_FORMATS");
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "NUM_FP_FORMATS");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "5");
}

TEST_F(ParamOverload3Test, FpFormatBitsIsClog2OfNumFpFormats) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = getParam(pkg, "FP_FORMAT_BITS");
  ASSERT_NE(p, nullptr);
  ExpectIntUnsigned(p->getTypespec(), "FP_FORMAT_BITS");
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "FP_FORMAT_BITS");
  ASSERT_NE(pa, nullptr);
  const hldb::SysFuncCall *const call = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "unreduced, the value is a system function call";
  EXPECT_EQ(call->getName(), "$clog2") << "20.8.1";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = call->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr) << "the argument is the name 'NUM_FP_FORMATS'";
  EXPECT_EQ(arg->getName(), "NUM_FP_FORMATS");
  EXPECT_EQ(arg->getActual(), getParam(pkg, "NUM_FP_FORMATS")) << "the package's own localparam";
}

TEST_F(ParamOverload3Test, FpEncodingTIsPackedStructOfTwoIntUnsigned) {
  const hldb::Typedef *const td = getTypedef("fp_encoding_t");
  ASSERT_NE(td, nullptr) << "typedef 'fp_encoding_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: fp_encoding_t is declared 'packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
  const std::string_view names[] = {"exp_bits", "man_bits"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(i);
    EXPECT_EQ(m->getName(), names[i]) << "member " << i;
    ExpectIntUnsigned(m->getTypespec(), names[i]);
  }
}

TEST_F(ParamOverload3Test, FpFormatEHasFiveConstants) {
  const hldb::Typedef *const td = getTypedef("fp_format_e");
  ASSERT_NE(td, nullptr) << "typedef 'fp_format_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: fp_format_e is an enumeration";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 5u);
  const std::pair<std::string_view, std::string_view> consts[] = {
      {"FP32", "'d0"}, {"FP64", "'d1"}, {"FP16", "'d2"}, {"FP8", "'d3"}, {"FP16ALT", "'d4"}};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    EXPECT_EQ(ec->getName(), consts[i].first) << "constant " << i;
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << consts[i].first << " is given a literal";
    EXPECT_EQ(value->getDecompile(), consts[i].second) << consts[i].first;
  }
}

TEST_F(ParamOverload3Test, FpFormatEBaseIsFpFormatBitsMinusOneToZero) {
  const hldb::Typedef *const td = getTypedef("fp_format_e");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "6.19: the base type is a logic";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  const hldb::Operation *const left = base->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound is 'FP_FORMAT_BITS-1'";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const bits = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(bits, nullptr);
  EXPECT_EQ(bits->getName(), "FP_FORMAT_BITS");
  EXPECT_EQ(bits->getActual(), getParam(getPkg(), "FP_FORMAT_BITS"));
  const hldb::Constant *const right = base->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ParamOverload3Test, FpEncodingsIsFiveNestedPatterns) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "FP_ENCODINGS");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const outer = pa->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr) << "unreduced, the value is an assignment pattern";
  EXPECT_EQ(outer->getOpType(), vpiAssignmentPatternOp) << "10.9.1";
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 5u) << "one item per format";
  const std::pair<std::string_view, std::string_view> items[] = {
      {"8", "23"}, {"11", "52"}, {"5", "10"}, {"5", "2"}, {"8", "7"}};
  for (size_t i = 0; i < 5; ++i) {
    SCOPED_TRACE(testing::Message() << "item " << i);
    const hldb::Operation *const inner = any_cast<hldb::Operation>(outer->getOperands()->at(i));
    ASSERT_NE(inner, nullptr) << "each item is a nested pattern";
    EXPECT_EQ(inner->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a positional structure pattern";
    ASSERT_NE(inner->getOperands(), nullptr);
    ASSERT_EQ(inner->getOperands()->size(), 2u);
    const hldb::Constant *const exp = any_cast<hldb::Constant>(inner->getOperands()->at(0));
    ASSERT_NE(exp, nullptr);
    EXPECT_EQ(exp->getDecompile(), items[i].first);
    const hldb::Constant *const man = any_cast<hldb::Constant>(inner->getOperands()->at(1));
    ASSERT_NE(man, nullptr);
    EXPECT_EQ(man->getDecompile(), items[i].second);
  }
}

TEST_F(ParamOverload3Test, FpWidthIsAutomaticFunctionOfFormat) {
  const hldb::Function *const f = getFpWidth();
  ASSERT_NE(f, nullptr) << "function 'fp_width' not found in fpnew_pkg";
  EXPECT_TRUE(f->getAutomatic()) << "13.4.2: fp_width is declared 'automatic'";
  ExpectIntUnsigned(f->getReturn(), "fp_width's return type");
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  const hldb::IODecl *const fmt = f->getIODecls()->at(0);
  EXPECT_EQ(fmt->getName(), "fmt");
  EXPECT_EQ(fmt->getDirection(), vpiInput) << "13.4: an ANSI argument with no direction is an input";
  EXPECT_TRUE(namesTypedef(fmt->getTypespec(), "fp_format_e")) << "'fmt' is declared 'fp_format_e'";
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamOverload3Test, TopHasOneNullPort) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(top->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  EXPECT_EQ(top->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(top->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamOverload3Test, NumFormatsIsScopedNumFpFormats) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Parameter *const p = getParam(top, "NUM_FORMATS");
  ASSERT_NE(p, nullptr) << "localparam 'NUM_FORMATS' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: 'NUM_FORMATS' is declared 'localparam'";
  ExpectIntUnsigned(p->getTypespec(), "NUM_FORMATS");
  const hldb::ParamAssign *const pa = getParamAssign(top, "NUM_FORMATS");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const path = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "unreduced, the value is the scoped name fpnew_pkg::NUM_FP_FORMATS";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the item";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "fpnew_pkg");
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names fpnew_pkg";
  const hldb::RefObj *const item = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getName(), "NUM_FP_FORMATS");
  ASSERT_NE(getParam(getPkg(), "NUM_FP_FORMATS"), nullptr);
  EXPECT_EQ(item->getActual(), getParam(getPkg(), "NUM_FP_FORMATS")) << "26.3: declared in fpnew_pkg";
}

TEST_F(ParamOverload3Test, LoopRunsWhileFmtBelowIntCastOfNumFormats) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr) << "27.4: top's only generate construct is a loop";
  const hldb::Operation *const lt = loop->getCondition<hldb::Operation>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.4.4: 'fmt < ...'";
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  const hldb::RefObj *const fmt = any_cast<hldb::RefObj>(lt->getOperands()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getName(), "fmt");
  const hldb::Operation *const cast = any_cast<hldb::Operation>(lt->getOperands()->at(1));
  ASSERT_NE(cast, nullptr) << "the bound is the cast int'(NUM_FORMATS)";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1";
  ASSERT_NE(cast->getTypespec(), nullptr);
  EXPECT_NE(cast->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "the casting type is int";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::RefObj *const num = any_cast<hldb::RefObj>(cast->getOperands()->at(0));
  ASSERT_NE(num, nullptr);
  EXPECT_EQ(num->getName(), "NUM_FORMATS");
  EXPECT_EQ(num->getActual(), getParam(getTop(), "NUM_FORMATS")) << "top's own localparam";
}

TEST_F(ParamOverload3Test, LoopBlockIsGenNanboxCheck) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::GenBlock *const block = loop->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr) << "the loop writes a 'begin ... end' block";
  EXPECT_EQ(block->getName(), "gen_nanbox_check") << "27.4: the block is named";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// type only warned) in the file header.
TEST_F(ParamOverload3Test, UndeclaredFpuFeaturesTIsAnError) {
  EXPECT_TRUE(reportsErrorNaming("fpu_features_t"))
      << "6.18: The declaration of a user-defined data type shall precede any reference to its type_identifier";
}

TEST_F(ParamOverload3Test, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
