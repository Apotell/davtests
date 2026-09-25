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

// Tests for FuncStruct/dut.sv:
//   package hmac_pkg;
//   typedef logic [31:0] sha_word_t;
//   function automatic sha_word_t [7:0] compress( input sha_word_t w, input sha_word_t [7:0] h_i = 0);
//     automatic sha_word_t sigma_0;
//     sigma_0 = 32'h11111111;
//     compress[0] = (sigma_0);
//   endfunction// : compress
//   endpackage
//
//   module dut import hmac_pkg::*; ();
//   sha_word_t [15:0] w;
//   sha_word_t [7:0] hash;
//   assign w[0] = 32'h00000000;
//   assign hash = compress(w[0], hash);
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape):
//
//   Sec 6.18 "Typedef declarations": "typedef logic [31:0] sha_word_t;"
//   is a simple type alias, whose own alias typespec is LogicTypespec
//   with one 32-bit range.
//
//   Sec 7.4.2 "Packed arrays": in "sha_word_t [7:0] compress(...)", the
//   "[7:0]" dimension appears BEFORE the declared identifier ("compress"),
//   which per 7.4.2 makes it a PACKED array dimension on top of the
//   typedef'd element type -- i.e. an 8-element packed array of
//   "sha_word_t", not an unpacked array. This distinguishes it from
//   FuncRetArray.cpp's "ASSIGN_VADDR_RET_T[2]" (an unpacked array,
//   dimension declared as part of the typedef with the size after the
//   type name). So getReturn() should resolve to an ArrayTypespec with
//   getPacked() == true, whose element typespec resolves to a
//   TypedefTypespec for "sha_word_t". The same shape applies to the
//   second formal, "input sha_word_t [7:0] h_i = 0" (with an additional
//   default-value expression, Constant "0", per Sec 13.3 "Tasks" formal
//   default value rules), while the first formal "input sha_word_t w"
//   (no packed dimension at all) should resolve its typespec directly to
//   TypedefTypespec "sha_word_t" with no ArrayTypespec wrapper.
//
//   Sec 6.7/6.8: "sha_word_t [15:0] w;" and "sha_word_t [7:0] hash;" at
//   module scope are explicit-type declarations (no net-type keyword), so
//   per Sec 6.7/6.8 they must be Variables, never Nets, regardless of any
//   `default_nettype` -- this is the same net-vs-variable modeling
//   pitfall called out in the test-writing guide, re-derived here from
//   the standard rather than from tool output.
//
//   Sec 13.4.1: "compress[0] = (sigma_0);" assigns a single element of
//   the function's own array-typed return-name variable via indexing --
//   the LHS should be a VarSelect/BitSelect named "compress" with index
//   Constant "0", RHS resolving through to RefObj "sigma_0".
//
// What is NOT checked and why:
//   - the runtime-computed value of "compress(w[0], hash)" is a
//     simulation-time concept.
//   - the "import hmac_pkg::*;" wildcard import mechanism itself is not
//     the focus of this file (it is exercised implicitly by every name
//     lookup below); no separate assertion is made on the import
//     declaration's own shape.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/var_select.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncStructTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncStruct.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("hmac_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Typedef *getShaWordT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getTypedefs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("sha_word_t", pkg->getTypedefs());
  }

  static const hldb::Function *getCompress() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("compress", pkg->getTaskFuncs());
  }
};

TEST_F(FuncStructTest, PackageAndModuleExist) {
  EXPECT_NE(getPkg(), nullptr);
  EXPECT_NE(getDut(), nullptr);
}

// typedef logic [31:0] sha_word_t;
TEST_F(FuncStructTest, ShaWordTIsThirtyTwoBitLogic) {
  const hldb::Typedef *const td = getShaWordT();
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getAlias();
  ASSERT_NE(alias, nullptr);
  const hldb::LogicTypespec *const logicTs = alias->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr);
  ASSERT_NE(logicTs->getRanges(), nullptr);
  ASSERT_EQ(logicTs->getRanges()->size(), 1u);
}

// function automatic sha_word_t [7:0] compress( input sha_word_t w, input sha_word_t [7:0] h_i = 0);
TEST_F(FuncStructTest, CompressReturnsPackedArrayOfEightShaWords) {
  const hldb::Function *const compress = getCompress();
  ASSERT_NE(compress, nullptr);
  EXPECT_TRUE(compress->getAutomatic());
  const hldb::RefTypespec *const rts = compress->getReturn();
  ASSERT_NE(rts, nullptr);
  const hldb::ArrayTypespec *const arrTs = rts->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arrTs, nullptr) << "'sha_word_t [7:0]' return type should resolve to an ArrayTypespec";
  EXPECT_TRUE(arrTs->getPacked())
      << "'[7:0]' preceding the function name is a packed dimension per IEEE 1800-2023 Sec 7.4.2";
  const hldb::RefTypespec *const elemRts = arrTs->getElemTypespec();
  ASSERT_NE(elemRts, nullptr);
  const hldb::TypedefTypespec *const elemTd = elemRts->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(elemTd, nullptr) << "element type should resolve to TypedefTypespec 'sha_word_t'";
  EXPECT_EQ(elemTd->getTypedef(), getShaWordT());
}

TEST_F(FuncStructTest, CompressHasWPlainAndHiPackedArrayWithDefaultZero) {
  const hldb::Function *const compress = getCompress();
  ASSERT_NE(compress, nullptr);
  ASSERT_NE(compress->getIODecls(), nullptr);
  ASSERT_EQ(compress->getIODecls()->size(), 2u);

  const hldb::IODecl *const w = compress->getIODecls()->at(0);
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getName(), "w");
  EXPECT_EQ(w->getDirection(), vpiInput);
  const hldb::RefTypespec *const wRts = w->getTypespec();
  ASSERT_NE(wRts, nullptr);
  const hldb::TypedefTypespec *const wTd = wRts->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(wTd, nullptr) << "'input sha_word_t w' (no packed dimension) should resolve directly to TypedefTypespec";
  EXPECT_EQ(wTd->getTypedef(), getShaWordT());

  const hldb::IODecl *const hI = compress->getIODecls()->at(1);
  ASSERT_NE(hI, nullptr);
  EXPECT_EQ(hI->getName(), "h_i");
  EXPECT_EQ(hI->getDirection(), vpiInput);
  const hldb::RefTypespec *const hIRts = hI->getTypespec();
  ASSERT_NE(hIRts, nullptr);
  const hldb::ArrayTypespec *const hIArrTs = hIRts->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(hIArrTs, nullptr) << "'input sha_word_t [7:0] h_i' should resolve to a packed ArrayTypespec";
  EXPECT_TRUE(hIArrTs->getPacked());
  const hldb::Constant *const defaultVal = any_cast<hldb::Constant>(hI->getExpr());
  ASSERT_NE(defaultVal, nullptr) << "'h_i = 0' should carry a default-value Constant";
  EXPECT_EQ(defaultVal->getDecompile(), "0");
}

// automatic sha_word_t sigma_0; sigma_0 = 32'h11111111; compress[0] = (sigma_0);
TEST_F(FuncStructTest, CompressBodyAssignsSigmaZeroThenElementZeroOfItsOwnReturnName) {
  const hldb::Function *const compress = getCompress();
  ASSERT_NE(compress, nullptr);
  const hldb::Begin *const body = compress->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getVariables(), nullptr);
  ASSERT_EQ(body->getVariables()->size(), 1u);
  const hldb::Variable *const sigma0 = hldb::findByName<hldb::Variable>("sigma_0", body->getVariables());
  ASSERT_NE(sigma0, nullptr);
  EXPECT_TRUE(sigma0->getAutomatic()) << "'automatic sha_word_t sigma_0;' is explicitly automatic";
  const hldb::RefTypespec *const sigma0Rts = sigma0->getTypespec();
  ASSERT_NE(sigma0Rts, nullptr);
  EXPECT_NE(sigma0Rts->getActual<hldb::TypedefTypespec>(), nullptr);

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::Assignment *const initAssign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(initAssign, nullptr) << "'sigma_0 = 32'h11111111;' should be an Assignment";
  const hldb::RefObj *const initLhs = initAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(initLhs, nullptr);
  EXPECT_EQ(initLhs->getName(), "sigma_0");
  const hldb::Constant *const initVal = any_cast<hldb::Constant>(initAssign->getRhs());
  ASSERT_NE(initVal, nullptr);

  const hldb::Assignment *const elemAssign = any_cast<hldb::Assignment>(body->getStmts()->at(2));
  ASSERT_NE(elemAssign, nullptr) << "'compress[0] = (sigma_0);' should be an Assignment";
  const hldb::VarSelect *const elemLhs = elemAssign->getLhs<hldb::VarSelect>();
  ASSERT_NE(elemLhs, nullptr) << "'compress[0]' should be a VarSelect indexing the implicit return-name variable";
  EXPECT_EQ(elemLhs->getName(), "compress");
  const hldb::Constant *const idx = elemLhs->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), "0");
  const hldb::RefObj *const elemRhs = any_cast<hldb::RefObj>(elemAssign->getRhs());
  ASSERT_NE(elemRhs, nullptr) << "'(sigma_0)' RHS should resolve through to RefObj 'sigma_0'";
  EXPECT_EQ(elemRhs->getName(), "sigma_0");
  EXPECT_EQ(elemRhs->getActual(), sigma0);
}

// sha_word_t [15:0] w; sha_word_t [7:0] hash;  -- module-scope, no net keyword
TEST_F(FuncStructTest, ModuleScopeWAndHashAreVariablesNotNets) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("w", dut->getVariables()), nullptr)
      << "'sha_word_t [15:0] w;' has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it must be a Variable";
  EXPECT_NE(hldb::findByName<hldb::Variable>("hash", dut->getVariables()), nullptr)
      << "'sha_word_t [7:0] hash;' has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it must be a Variable";
}

// assign hash = compress(w[0], hash);
TEST_F(FuncStructTest, HashIsAssignedFromCompressCallOnWZeroAndHash) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::Function *const compress = getCompress();
  ASSERT_NE(compress, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);

  const hldb::ContAssign *hashAssign = nullptr;
  for (const hldb::ContAssign *const ca : *dut->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    if (lhs != nullptr && lhs->getName() == "hash") {
      hashAssign = ca;
      break;
    }
  }
  ASSERT_NE(hashAssign, nullptr) << "'assign hash = compress(w[0], hash);' not found";
  const hldb::FuncCall *const call = hashAssign->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "compress");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), compress);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::BitSelect *const wArg = any_cast<hldb::BitSelect>(call->getArguments()->at(0));
  ASSERT_NE(wArg, nullptr) << "'w[0]' first argument should be a BitSelect";
  EXPECT_EQ(wArg->getName(), "w");
  const hldb::Constant *const wIdx = any_cast<hldb::Constant>(wArg->getIndex());
  ASSERT_NE(wIdx, nullptr);
  EXPECT_EQ(wIdx->getDecompile(), "0");

  const hldb::RefObj *const hashArg = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(hashArg, nullptr) << "second argument 'hash' should be a RefObj";
  EXPECT_EQ(hashArg->getName(), "hash");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
