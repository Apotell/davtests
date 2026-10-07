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

// Tests for tests/ArianeElab2/dut.sv (tags: ArianeElab2).
//
// dut.sv here is the same trimmed Ariane core as tests/ArianeElab/dut.sv
// (see test_ArianeElab.cpp for the riscv/ariane_pkg package, enum, and
// top->ariane->ex_stage->fpu_wrap instance-hierarchy coverage, all of
// which is unchanged and not re-tested here) plus ~80 added lines: a new
// "fpnew_opgroup_block" module and a rewritten "fpnew_top" that instantiate
// an FPU format-slice hierarchy:
//   module fpnew_opgroup_block #( ... ) ();
//     for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++) begin : gen_parallel_slices
//       ...
//       if (FpFmtMask[fmt] && (FmtUnitTypes[fmt] == fpnew_pkg::PARALLEL)) begin : active_format
//         ...
//         fpnew_opgroup_fmt_slice #( ... ) i_fmt_slice ();
//       end
//     end
//   endmodule
//   module fpnew_top #( ... ) ();
//     for (genvar opgrp = 0; opgrp < int'(NUM_OPGROUPS); opgrp++) begin : gen_operation_groups
//       fpnew_opgroup_block #( ... ) i_opgroup_block ();
//     end
//   endmodule
//
// "fpnew_opgroup_fmt_slice" is never defined anywhere in this trimmed
// dut.sv (unlike "fpnew_opgroup_block"/"fpnew_top", which are real modules
// here) -- this is the same kind of genuinely-unresolved reference as
// "ariane_soc" in the ArianeElab test, except this one is inside a
// "generate-for" body rather than a plain parameter override, and HLC's
// handling of that combination is exactly what this file exists to pin
// down: the unresolved reference does not just produce one isolated
// DB2029 "unsupported typespec" error -- it also makes HLC report a
// DB2038 "'vpiStmt' property value has invalid objects" error at every
// enclosing generate construct's own source location: fpnew_opgroup_block's
// "for" loop (1974:55), fpnew_top's "for" loop (2019:64), and fpu_wrap's
// "if (FP_PRESENT) begin : fpu_gen" (2043:19) -- i.e. exactly the chain of
// generate scopes that nest the bad reference, bottom to top.
//
// Checked:
//   - module "fpnew_opgroup_block" exists (matched by its bare vpiDefName,
//     since its decorated vpiName embeds its unelaborated default
//     parameter values, same as in the AlwaysNoElab test)
//   - its "for (genvar fmt = 0; fmt < int'(NUM_FORMATS); fmt++)" is the
//     module's only generate statement, stored directly as a GenFor (no
//     surrounding GenRegion, since the source has no "generate"/
//     "endgenerate" keywords); the "int'(...)" cast resolves cleanly to a
//     builtin IntTypespec (unlike AlwaysNoElab's parameter-named cast,
//     which could not resolve without elaboration)
//   - that GenFor's generated block (named "gen_parallel_slices") contains
//     a nested GenIf ("active_format"), whose body contains a RefInstance
//     "i_fmt_slice" of "fpnew_opgroup_fmt_slice" -- resolved to an
//     UnsupportedTypespec, since that module is never defined
//   - compiling dut.sv reports both the expected HLDB_UNSUPPORTED_TYPESPEC
//     for "fpnew_opgroup_fmt_slice" (at its own instantiation site) and
//     the 3 cascading HLDB_ILLEGAL_PROPERTY_VALUE errors at the 3 nested
//     generate constructs listed above
//
// NOT CHECKED: whether HLC's handling here is "correct" per any IEEE
// 1800-2023 rule (generate constructs over an unresolved module are not
// something the standard specifies compiler-internal error recovery for),
// and the exact value NUM_FORMATS/NUM_OPGROUPS would fold to (both are
// package-function calls over types this trimmed file does define, but
// folding them was not needed to pin down the bug this file documents, so
// it was not pursued further here).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/unsupported_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArianeElab2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ArianeElab2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getOpgroupBlockModule() {
    return hldb::findByDefName<hldb::Module>("fpnew_opgroup_block", m_design->getAllModules());
  }

  static const hldb::GenFor *getFormatLoop() {
    const hldb::Module *const mod = getOpgroupBlockModule();
    if (mod == nullptr || mod->getGenStmts() == nullptr || mod->getGenStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::GenFor>(mod->getGenStmts()->at(0));
  }

  // Scan a Begin's statements for the first child of type T (mirrors the
  // lookup pattern test_ArianeElab.cpp uses for ex_stage's GenRegion/GenIf).
  template <typename T>
  static const T *findFirst(const hldb::Begin *begin) {
    if (begin == nullptr || begin->getStmts() == nullptr) {
      return nullptr;
    }
    for (const hldb::Any *const stmt : *begin->getStmts()) {
      if (const T *const match = any_cast<T>(stmt)) {
        return match;
      }
    }
    return nullptr;
  }

  static const hldb::RefInstance *getFmtSliceInstance() {
    const hldb::GenFor *const genFor = getFormatLoop();
    if (genFor == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const sliceLoopBody = genFor->getStmt<hldb::Begin>();
    const hldb::GenIf *const activeFormat = findFirst<hldb::GenIf>(sliceLoopBody);
    if (activeFormat == nullptr) {
      return nullptr;
    }
    return findFirst<hldb::RefInstance>(activeFormat->getStmt<hldb::Begin>());
  }
};

// --- fpnew_opgroup_block's generate-for --------------------------------------

TEST_F(ArianeElab2Test, OpgroupBlockModuleExists) { EXPECT_NE(getOpgroupBlockModule(), nullptr); }

TEST_F(ArianeElab2Test, FormatLoopIsASolitaryGenForCastingNumFormatsToInt) {
  const hldb::Module *const mod = getOpgroupBlockModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getGenStmts(), nullptr);
  ASSERT_EQ(mod->getGenStmts()->size(), 1u)
      << "the module's only generate construct is its top-level 'for (genvar fmt ...)' loop";

  const hldb::GenFor *const genFor = getFormatLoop();
  ASSERT_NE(genFor, nullptr) << "should be a GenFor directly, with no GenRegion wrapper";

  ASSERT_NE(genFor->getVariables(), nullptr);
  ASSERT_EQ(genFor->getVariables()->size(), 1u);
  const hldb::Variable *const fmt = genFor->getVariables()->at(0);
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getName(), "fmt");

  ASSERT_NE(genFor->getForInitStmts(), nullptr);
  ASSERT_EQ(genFor->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(genFor->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getLhs<hldb::Variable>(), fmt) << "for-init's lhs is the genvar Variable directly, not a RefObj";
  const hldb::Constant *const initVal = init->getRhs<hldb::Constant>();
  ASSERT_NE(initVal, nullptr);
  EXPECT_EQ(initVal->getDecompile(), "0");

  const hldb::Operation *const condition = genFor->getCondition<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiLtOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 2u);
  const hldb::RefObj *const fmtRef = any_cast<hldb::RefObj>(condition->getOperands()->at(0));
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_EQ(fmtRef->getActual<hldb::Variable>(), fmt);

  const hldb::Operation *const cast = any_cast<hldb::Operation>(condition->getOperands()->at(1));
  ASSERT_NE(cast, nullptr) << "'int'(NUM_FORMATS)' should be a cast Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp);
  ASSERT_NE(cast->getTypespec(), nullptr);
  EXPECT_NE(cast->getTypespec()->getActual<hldb::IntTypespec>(), nullptr)
      << "casting to the builtin 'int' should resolve cleanly, unlike a parameter-named cast target";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::RefObj *const numFormats = any_cast<hldb::RefObj>(cast->getOperands()->at(0));
  ASSERT_NE(numFormats, nullptr);
  EXPECT_EQ(numFormats->getName(), "NUM_FORMATS");
  EXPECT_NE(numFormats->getActual<hldb::Parameter>(), nullptr);

  ASSERT_NE(genFor->getForIncStmts(), nullptr);
  ASSERT_EQ(genFor->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(genFor->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);

  const hldb::Begin *const body = genFor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->getName(), "gen_parallel_slices");
}

TEST_F(ArianeElab2Test, GeneratedBlockInstantiatesUnresolvedFmtSlice) {
  const hldb::RefInstance *const inst = getFmtSliceInstance();
  ASSERT_NE(inst, nullptr) << "'fpnew_opgroup_fmt_slice #(...) i_fmt_slice ();' not found inside the nested "
                              "'active_format' generate-if";
  EXPECT_EQ(inst->getName(), "i_fmt_slice");
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::UnsupportedTypespec *const ts = inst->getTypespec()->getActual<hldb::UnsupportedTypespec>();
  ASSERT_NE(ts, nullptr) << "'fpnew_opgroup_fmt_slice' is never defined in this trimmed file";
  EXPECT_EQ(ts->getName(), "fpnew_opgroup_fmt_slice");
}

// --- compiler diagnostics: the unresolved reference cascades upward ---------

TEST_F(ArianeElab2Test, UnresolvedFmtSliceReportsUnsupportedTypespec) {
  EXPECT_NE(findError(ErrorDefinition::HLDB_UNSUPPORTED_TYPESPEC, "fpnew_opgroup_fmt_slice"), nullptr);
}

TEST_F(ArianeElab2Test, UnresolvedReferenceCascadesToEveryEnclosingGenerateConstruct) {
  // fpnew_opgroup_block's "for (genvar fmt ...)":
  EXPECT_NE(findError(ErrorDefinition::HLDB_ILLEGAL_PROPERTY_VALUE, 1974, 55), nullptr)
      << "fpnew_opgroup_block's generate-for should report the cascading 'vpiStmt' error";
  // fpnew_top's "for (genvar opgrp ...)", which instantiates fpnew_opgroup_block:
  EXPECT_NE(findError(ErrorDefinition::HLDB_ILLEGAL_PROPERTY_VALUE, 2019, 64), nullptr)
      << "fpnew_top's generate-for should also report it, one level further up the instance chain";
  // fpu_wrap's "if (FP_PRESENT) begin : fpu_gen", which instantiates fpnew_top:
  EXPECT_NE(findError(ErrorDefinition::HLDB_ILLEGAL_PROPERTY_VALUE, 2043, 19), nullptr)
      << "fpu_wrap's generate-if should also report it, at the top of the instance chain";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
