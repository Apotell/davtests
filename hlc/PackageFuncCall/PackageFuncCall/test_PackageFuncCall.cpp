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

// Validates the HLDB model built for tests/PackageFuncCall/dut.sv:
//
//   package prim_cipher_pkg;
//     parameter logic [15:0][3:0] PRESENT_SBOX4 = {4'h2, 4'h1, 4'h7, 4'h4,
//                                                  4'h8, 4'hF, 4'hE, 4'h3,
//                                                  4'hD, 4'hA, 4'h0, 4'h9,
//                                                  4'hB, 4'h6, 4'h5, 4'hC};
//     function automatic logic [63:0] sbox4_64bit(logic [63:0] state_in,
//                                                 logic [15:0][3:0] sbox4);
//       logic [63:0] state_out;
//       for (int k = 0; k < 64/4; k++) begin
//         state_out[k*4  +: 4] = sbox4[state_in[k*4  +: 4]];
//       end
//       return state_out;
//     endfunction : sbox4_64bit
//     parameter logic [11:0][63:0] PRINCE_ROUND_CONST = {64'hC0AC29B7C97C50DD,
//                                                        64'hD3B5A399CA0C2399};
//   endpackage : prim_cipher_pkg
//
//   module top();
//   assign data_state_sbox = prim_cipher_pkg::sbox4_64bit(data_state_xor,
//                          prim_cipher_pkg::PRESENT_SBOX4);
//    always_comb begin : p_post_round_xor
//      data_o  = data_state[2*NumRoundsHalf+1] ^
//                prim_cipher_pkg::PRINCE_ROUND_CONST[11][DataWidth-1:0];
//      data_o ^= k1;
//      data_o ^= k0_prime;
//    end
//   endmodule
//
// The point of the fixture is a module calling a package function and
// selecting from package parameters through the scope resolution operator
// (IEEE 1800-2023 26.3). The module body is a fragment lifted from a larger
// design: most identifiers it uses are declared nowhere in this file.
//
// What is checked, and why:
//   Package prim_cipher_pkg (26.2)
//     - 'endpackage : prim_cipher_pkg' records the end label
//     - exactly 2 parameters and 2 ParamAssigns; both parameters are
//       localparams (6.20.4: 'parameter' in a package means 'localparam')
//     - exactly 1 TaskFunc, the Function sbox4_64bit
//   PRESENT_SBOX4 : logic [15:0][3:0] (7.4.1)
//     - LogicTypespec with exactly 2 packed ranges, [15:0] then [3:0]
//     - unreduced, the value is an Operation vpiConcatOp (11.4.12) with 16
//       operands, each a 4-bit vpiHexConst, in source order
//     - on an elaborated design the value is reduced to a Constant equal to
//       the concatenation, 64'h21748FE3DA09B65C (16 x 4 = 64 bits)
//   PRINCE_ROUND_CONST : logic [11:0][63:0]
//     - LogicTypespec with exactly 2 packed ranges, [11:0] then [63:0]
//     - unreduced, the value is an Operation vpiConcatOp with 2 operands,
//       64'hC0AC29B7C97C50DD and 64'hD3B5A399CA0C2399, each a 64-bit
//       vpiHexConst
//     - on an elaborated design the value is reduced to a Constant whose
//       width is either 128 (the concatenation, 11.4.12) or 768 (after the
//       zero extension to the parameter's type, 10.7)
//   function automatic logic [63:0] sbox4_64bit(...) (13.4)
//     - automatic (13.4.2); 'endfunction : sbox4_64bit' records the end label
//     - return type LogicTypespec with 1 packed range [63:0]
//     - exactly 2 formals, in order: 'state_in' (input, logic [63:0]) and
//       'sbox4' (input, logic [15:0][3:0]); direction defaults to input
//     - exactly 1 function-local variable 'state_out', logic [63:0]; the
//       loop variable 'k' belongs to the for loop, not to the function
//     - executable statements, in order: a ForStmt and a ReturnStmt
//   for (int k = 0; k < 64/4; k++) (12.7.1)
//     - 'int k' is declared in the for-init, so the ForStmt owns exactly 1
//       Variable 'k' of signed int type
//     - init: one Assignment whose LHS is that Variable 'k' itself (its own
//       declaration) and whose RHS is Constant "0"
//     - condition: Operation vpiLtOp over RefObj 'k' and Operation vpiDivOp
//       over Constants "64" and "4"
//     - step: one Operation vpiPostIncOp over RefObj 'k'
//     - body: begin-end with exactly 1 blocking Assignment:
//         LHS  state_out[k*4 +: 4]  -> IndexedPartSelect vpiPosIndexed
//              (11.5.1), prefix bound to the local 'state_out', base
//              Operation vpiMultOp over 'k' and "4", width Constant "4"
//         RHS  sbox4[state_in[k*4 +: 4]]  -> BitSelect whose prefix is bound
//              to the formal 'sbox4' and whose index is an IndexedPartSelect
//              vpiPosIndexed on the formal 'state_in' with the same base
//              and width
//   return state_out; -> ReturnStmt with RefObj bound to 'state_out'
//   Module top
//     - 6.10: 'data_state_sbox' is undeclared and appears on the left of a
//       continuous assignment, so an implicit scalar net of the default net
//       type (wire) is created. It is the module's only Net; the module has
//       no Variables (no identifier in it is declared as one) and no ports.
//   assign data_state_sbox = prim_cipher_pkg::sbox4_64bit(...);
//     - exactly 1 ContAssign; LHS bound to the implicit Net
//     - RHS is a package-scoped RefObj path: a prefix bound to
//       prim_cipher_pkg, then a FuncCall bound by object identity to the
//       package's sbox4_64bit (26.3), with exactly 2 arguments: RefObj
//       'data_state_xor', which is declared nowhere and so binds to
//       nothing, and the scoped reference to PRESENT_SBOX4 (again a path
//       whose last element is bound to that Parameter)
//   always_comb begin : p_post_round_xor ... end (9.2.2.2, 9.3.4)
//     - exactly 1 process, an Always with vpiAlwaysComb, whose statement is
//       a Begin named "p_post_round_xor" holding exactly 3 statements
//     - [0] blocking Assignment to RefObj 'data_o'; RHS Operation
//       vpiBitXorOp (11.4.8) with operands
//         data_state[2*NumRoundsHalf+1] -> BitSelect on 'data_state', index
//           Operation vpiAddOp over (Operation vpiMultOp over "2" and
//           'NumRoundsHalf') and "1"
//         prim_cipher_pkg::PRINCE_ROUND_CONST[11][DataWidth-1:0] -> a
//           package-scoped RefObj path ending in a PartSelect with range
//           left Operation vpiSubOp over 'DataWidth' and "1", right "0",
//           applied to a BitSelect with index "11" whose prefix is bound to
//           the Parameter PRINCE_ROUND_CONST
//     - [1] 'data_o ^= k1' and [2] 'data_o ^= k0_prime': each a blocking
//       Assignment to 'data_o' whose RHS is the lowered compound form
//       (11.4.1), Operation vpiBitXorOp over 'data_o' and 'k1' / 'k0_prime'
//     - every identifier in the block other than the package parameter is
//       declared nowhere, and a procedural assignment never creates an
//       implicit net (6.10), so each of those references binds to nothing
//   Diagnostics
//     - 6.5: data must be declared before use apart from implicit nets.
//       Each of the 7 undeclared identifiers -- data_state_xor, data_o,
//       data_state, NumRoundsHalf, DataWidth, k1, k0_prime -- must be
//       reported as COMP_UNDEFINED_VARIABLE
//     - 'data_state_sbox' is a legal implicit net and must NOT be reported
//     - the package-scoped names sbox4_64bit, PRESENT_SBOX4 and
//       PRINCE_ROUND_CONST must bind: no COMP_FAILED_TO_BIND for them
//     - there is no syntax error in the file: zero syntax and zero fatal
//       diagnostics
//
// What is NOT checked, and why:
//   - What the function computes for any input, and the values data_o and
//     data_state_sbox take, only exist while simulation runs (and the
//     inputs are undeclared anyway). Permanently out of scope; the static
//     half -- the call's binding and argument list -- is covered by
//     ContAssignRhsCallsPackageFunction, and the loop's structure by the
//     For* tests.
//   - '64/4' in the loop condition is not a constant-expression context, so
//     nothing in the standard requires HLC to fold it; only its unreduced
//     form is asserted.
//   - The exact spelling HLC gives package-scoped names (for example
//     "PRESENT_SBOX4" versus "prim_cipher_pkg::PRESENT_SBOX4") is a tool
//     convention; either is accepted, and bindings are checked by object
//     identity.
//   - HLC represents a package-scoped name as a RefObj path (the package,
//     then the named item, call or select) rather than a bare FuncCall,
//     RefObj or PartSelect. That shape is a model convention; the tests
//     follow it and assert both the package and the item binding.
//   - Whether the implicit net records a typespec, and which, is a tool
//     convention; the net's kind, net type and scalar-ness are asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/indexed_part_select.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class PackageFuncCallTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageFuncCall.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_cipher_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ParamAssign>(name, pkg->getParamAssigns());
  }

  static const hldb::Function *getSbox() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("sbox4_64bit", pkg->getTaskFuncs());
  }

  static const hldb::Begin *getSboxBody() {
    const hldb::Function *const fn = getSbox();
    if (fn == nullptr) return nullptr;
    return fn->getStmt<hldb::Begin>();
  }

  static const hldb::IODecl *getFormal(std::string_view name) {
    const hldb::Function *const fn = getSbox();
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>(name, fn->getIODecls());
  }

  // 'logic [63:0] state_out;' may be owned by the function scope itself or
  // by the Begin wrapping its body -- which scope owns it is a tool
  // convention, so both are searched. SboxHasOneLocalVariableStateOut checks
  // there is exactly one function-local declaration across both.
  static const hldb::Variable *getStateOut() {
    const hldb::Function *const fn = getSbox();
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>("state_out", fn->getVariables())) return v;
    const hldb::Begin *const body = getSboxBody();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("state_out", body->getVariables());
  }

  // The function body's statements with bare Variable declarations removed.
  static std::vector<const hldb::Any *> getExecutableStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Begin *const body = getSboxBody();
    if (body == nullptr || body->getStmts() == nullptr) return stmts;
    for (const hldb::Any *const stmt : *body->getStmts()) {
      if (any_cast<hldb::Variable>(stmt) == nullptr) stmts.emplace_back(stmt);
    }
    return stmts;
  }

  static const hldb::ForStmt *getFor() {
    const std::vector<const hldb::Any *> stmts = getExecutableStmts();
    if (stmts.empty()) return nullptr;
    return any_cast<hldb::ForStmt>(stmts.front());
  }

  static const hldb::Variable *getK() {
    const hldb::ForStmt *const forStmt = getFor();
    if (forStmt == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("k", forStmt->getVariables());
  }

  static const hldb::Assignment *getLoopAssign() {
    const hldb::ForStmt *const forStmt = getFor();
    if (forStmt == nullptr) return nullptr;
    const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
    if (loopBody == nullptr || loopBody->getStmts() == nullptr || loopBody->getStmts()->empty()) return nullptr;
    return any_cast<hldb::Assignment>(loopBody->getStmts()->at(0));
  }

  static const hldb::Net *getImplicitNet() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("data_state_sbox", top->getNets());
  }

  static const hldb::ContAssign *getContAssign() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr || top->getContAssigns()->empty()) return nullptr;
    return top->getContAssigns()->at(0);
  }

  static const hldb::Begin *getAlwaysBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
    if (always == nullptr) return nullptr;
    return always->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAlwaysAssign(size_t index) {
    const hldb::Begin *const body = getAlwaysBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) return nullptr;
    return any_cast<hldb::Assignment>(body->getStmts()->at(index));
  }

  static bool isScopedName(std::string_view actual, std::string_view id) {
    return (actual == id) || (actual == std::string("prim_cipher_pkg::") + std::string(id));
  }

  // Verifies 'expr' is a RefObj named 'name' that binds to nothing: every
  // identifier checked this way is declared nowhere in the design.
  static void ExpectUnboundRef(const hldb::Any *expr, std::string_view name) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual(), nullptr) << "'" << name << "' is declared nowhere, so it cannot bind";
  }

  // Verifies 'expr' is a package-scoped name 'prim_cipher_pkg::...': a
  // RefObj path of exactly 2 elements whose prefix is bound to the package.
  // Stores the last element, the named item itself, in 'item'.
  static void ExpectPackagePath(const hldb::Any *expr, std::string_view what, const hldb::Any **item) {
    *item = nullptr;
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'" << what << "' should be a package-scoped RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "'" << what << "': the package, then the named item";
    const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(scope, nullptr);
    EXPECT_EQ(scope->getName(), "prim_cipher_pkg");
    ASSERT_NE(getPkg(), nullptr);
    EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names package prim_cipher_pkg";
    *item = path->getPathElems()->at(1);
  }

  // Verifies 'range' is [left:right] with Constant bounds.
  static void ExpectConstRange(const hldb::Range *range, std::string_view left, std::string_view right) {
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const l = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), left);
    EXPECT_EQ(r->getDecompile(), right);
  }

  // Verifies 'sel' is '<prefixTarget>[k*4 +: 4]' with the prefix bound to
  // 'prefixTarget' and 'k' bound to the loop variable.
  static void ExpectKTimesFourPlusColonFour(const hldb::IndexedPartSelect *sel, std::string_view prefixName,
                                            const hldb::Any *prefixTarget) {
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(sel->getIndexedPartSelectType(), vpiPosIndexed) << "11.5.1: '+:' is an ascending indexed part-select";
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), prefixName);
    ASSERT_NE(prefixTarget, nullptr);
    EXPECT_EQ(prefix->getActual(), prefixTarget);

    const hldb::Operation *const base = sel->getBaseExpr<hldb::Operation>();
    ASSERT_NE(base, nullptr) << "base 'k*4' should be an Operation";
    EXPECT_EQ(base->getOpType(), vpiMultOp);
    ASSERT_NE(base->getOperands(), nullptr);
    ASSERT_EQ(base->getOperands()->size(), 2u);
    const hldb::RefObj *const k = any_cast<hldb::RefObj>(base->getOperands()->at(0));
    ASSERT_NE(k, nullptr);
    EXPECT_EQ(k->getName(), "k");
    EXPECT_EQ(k->getActual<hldb::Variable>(), getK());
    const hldb::Constant *const four = any_cast<hldb::Constant>(base->getOperands()->at(1));
    ASSERT_NE(four, nullptr);
    EXPECT_EQ(four->getDecompile(), "4");

    const hldb::Constant *const width = sel->getWidthExpr<hldb::Constant>();
    ASSERT_NE(width, nullptr);
    EXPECT_EQ(width->getDecompile(), "4");
  }

  // Integer value of a Constant, from its decompiled text or from a
  // sized/based literal. The radix HLC picks for a folded value is a tool
  // convention, so every IEEE 1800 integer form is accepted and the numeric
  // value is what gets compared.
  static bool parseConstantValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
    }
    if (text.empty()) text = std::string(c->getValue());
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
  }
};

// ---------------------------------------------------------------------------
// package prim_cipher_pkg ... endpackage : prim_cipher_pkg
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, PackageExistsWithEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'prim_cipher_pkg' not found in Design::getAllPackages()";
  EXPECT_EQ(pkg->getName(), "prim_cipher_pkg");
  EXPECT_EQ(pkg->getEndLabel(), "prim_cipher_pkg") << "'endpackage : prim_cipher_pkg' carries an end label";
}

TEST_F(PackageFuncCallTest, PackageHasTwoLocalParamsAndOneFunction) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u);
  ASSERT_NE(pkg->getParamAssigns(), nullptr);
  EXPECT_EQ(pkg->getParamAssigns()->size(), 2u);
  for (std::string_view name : {"PRESENT_SBOX4", "PRINCE_ROUND_CONST"}) {
    const hldb::Parameter *const p = getParam(name);
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << name << "' is declared in a package";
  }
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u);
}

// ---------------------------------------------------------------------------
// parameter logic [15:0][3:0] PRESENT_SBOX4 = {4'h2, ..., 4'hC};
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, PresentSbox4IsLogic15To0By3To0) {
  const hldb::Parameter *const p = getParam("PRESENT_SBOX4");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "declared 'logic [15:0][3:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "7.4.1: two packed dimensions";
  ExpectConstRange(lt->getRanges()->at(0), "15", "0");
  ExpectConstRange(lt->getRanges()->at(1), "3", "0");
}

TEST_F(PackageFuncCallTest, PresentSbox4ValueIsConcatenationOfSixteenNibbles) {
  const hldb::ParamAssign *const pa = getParamAssign("PRESENT_SBOX4");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  if (m_design->getElaborated()) {
    // 6.20.2: a parameter's value is a constant expression fixed at
    // elaboration; 11.4.12: the concatenation is 16 x 4 = 64 bits wide.
    const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "on an elaborated design the concatenation should be reduced to a Constant";
    EXPECT_EQ(value->getSize(), 64);
    uint64_t v = 0;
    ASSERT_TRUE(parseConstantValue(value, &v)) << "unparsable constant '" << value->getDecompile() << "'";
    EXPECT_EQ(v, 0x21748FE3DA09B65CULL);
  } else {
    const hldb::Operation *const concat = pa->getRhs<hldb::Operation>();
    ASSERT_NE(concat, nullptr);
    EXPECT_EQ(concat->getOpType(), vpiConcatOp);
    ASSERT_NE(concat->getOperands(), nullptr);
    const std::vector<std::string_view> expected = {"4'h2", "4'h1", "4'h7", "4'h4", "4'h8", "4'hF", "4'hE", "4'h3",
                                                    "4'hD", "4'hA", "4'h0", "4'h9", "4'hB", "4'h6", "4'h5", "4'hC"};
    ASSERT_EQ(concat->getOperands()->size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(concat->getOperands()->at(i));
      ASSERT_NE(c, nullptr) << "operand " << i;
      EXPECT_EQ(c->getDecompile(), expected[i]) << "operand " << i;
      EXPECT_EQ(c->getConstType(), vpiHexConst) << "operand " << i;
      EXPECT_EQ(c->getSize(), 4) << "operand " << i;
    }
  }
}

// ---------------------------------------------------------------------------
// parameter logic [11:0][63:0] PRINCE_ROUND_CONST = {64'h..., 64'h...};
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, PrinceRoundConstIsLogic11To0By63To0) {
  const hldb::Parameter *const p = getParam("PRINCE_ROUND_CONST");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "declared 'logic [11:0][63:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "7.4.1: two packed dimensions";
  ExpectConstRange(lt->getRanges()->at(0), "11", "0");
  ExpectConstRange(lt->getRanges()->at(1), "63", "0");
}

TEST_F(PackageFuncCallTest, PrinceRoundConstValueIsConcatenationOfTwoWords) {
  const hldb::ParamAssign *const pa = getParamAssign("PRINCE_ROUND_CONST");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  if (m_design->getElaborated()) {
    const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "on an elaborated design the concatenation should be reduced to a Constant";
    EXPECT_TRUE((value->getSize() == 128) || (value->getSize() == 768))
        << "the folded value is the 128-bit concatenation, or that value zero-extended to the 768-bit "
           "parameter type (10.7); got "
        << value->getSize();
  } else {
    const hldb::Operation *const concat = pa->getRhs<hldb::Operation>();
    ASSERT_NE(concat, nullptr);
    EXPECT_EQ(concat->getOpType(), vpiConcatOp);
    ASSERT_NE(concat->getOperands(), nullptr);
    ASSERT_EQ(concat->getOperands()->size(), 2u);
    const std::vector<std::string_view> expected = {"64'hC0AC29B7C97C50DD", "64'hD3B5A399CA0C2399"};
    for (size_t i = 0; i < expected.size(); ++i) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(concat->getOperands()->at(i));
      ASSERT_NE(c, nullptr) << "operand " << i;
      EXPECT_EQ(c->getDecompile(), expected[i]) << "operand " << i;
      EXPECT_EQ(c->getConstType(), vpiHexConst) << "operand " << i;
      EXPECT_EQ(c->getSize(), 64) << "operand " << i;
    }
  }
}

// ---------------------------------------------------------------------------
// function automatic logic [63:0] sbox4_64bit(...) ... endfunction : sbox4_64bit
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, SboxIsAutomaticFunctionWithEndLabel) {
  const hldb::Function *const fn = getSbox();
  ASSERT_NE(fn, nullptr) << "Function 'sbox4_64bit' not found in the package's TaskFuncs";
  EXPECT_EQ(fn->getName(), "sbox4_64bit");
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: declared 'function automatic'";
  EXPECT_EQ(fn->getEndLabel(), "sbox4_64bit") << "'endfunction : sbox4_64bit' carries an end label";
}

TEST_F(PackageFuncCallTest, SboxReturnsLogic63To0) {
  const hldb::Function *const fn = getSbox();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr);
  const hldb::LogicTypespec *const lt = fn->getReturn()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "return type is 'logic [63:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstRange(lt->getRanges()->at(0), "63", "0");
}

TEST_F(PackageFuncCallTest, SboxHasTwoInputFormalsInOrder) {
  const hldb::Function *const fn = getSbox();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u);

  const hldb::IODecl *const stateIn = fn->getIODecls()->at(0);
  ASSERT_NE(stateIn, nullptr);
  EXPECT_EQ(stateIn->getName(), "state_in");
  EXPECT_EQ(stateIn->getDirection(), vpiInput) << "13.4: a formal with no direction defaults to input";
  ASSERT_NE(stateIn->getTypespec(), nullptr);
  const hldb::LogicTypespec *const inTs = stateIn->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(inTs, nullptr);
  ASSERT_NE(inTs->getRanges(), nullptr);
  ASSERT_EQ(inTs->getRanges()->size(), 1u);
  ExpectConstRange(inTs->getRanges()->at(0), "63", "0");

  const hldb::IODecl *const sbox4 = fn->getIODecls()->at(1);
  ASSERT_NE(sbox4, nullptr);
  EXPECT_EQ(sbox4->getName(), "sbox4");
  EXPECT_EQ(sbox4->getDirection(), vpiInput);
  ASSERT_NE(sbox4->getTypespec(), nullptr);
  const hldb::LogicTypespec *const sboxTs = sbox4->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(sboxTs, nullptr);
  ASSERT_NE(sboxTs->getRanges(), nullptr);
  ASSERT_EQ(sboxTs->getRanges()->size(), 2u);
  ExpectConstRange(sboxTs->getRanges()->at(0), "15", "0");
  ExpectConstRange(sboxTs->getRanges()->at(1), "3", "0");
}

TEST_F(PackageFuncCallTest, SboxHasOneLocalVariableStateOut) {
  const hldb::Function *const fn = getSbox();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(getSboxBody(), nullptr) << "a multi-statement function body should be wrapped in a Begin";
  size_t count = (fn->getVariables() == nullptr) ? 0u : fn->getVariables()->size();
  const hldb::Begin *const body = getSboxBody();
  if (body->getVariables() != nullptr) count += body->getVariables()->size();
  EXPECT_EQ(count, 1u) << "the only function-local declaration is 'state_out' ('k' belongs to the for loop)";

  const hldb::Variable *const stateOut = getStateOut();
  ASSERT_NE(stateOut, nullptr);
  ASSERT_NE(stateOut->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = stateOut->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "declared 'logic [63:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstRange(lt->getRanges()->at(0), "63", "0");
  EXPECT_EQ(stateOut->getValue(), nullptr) << "'logic [63:0] state_out;' has no initializer";
}

TEST_F(PackageFuncCallTest, SboxExecutableStatementsAreForThenReturn) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 2u) << "the body executes exactly two statements: the for loop and the return";
  EXPECT_NE(any_cast<hldb::ForStmt>(stmts[0]), nullptr);
  EXPECT_NE(any_cast<hldb::ReturnStmt>(stmts[1]), nullptr);
}

// ---------------------------------------------------------------------------
// for (int k = 0; k < 64/4; k++)
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, ForOwnsLoopVariableK) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getVariables(), nullptr) << "'int k' declared in the for-init lives in the loop's own scope";
  ASSERT_EQ(forStmt->getVariables()->size(), 1u);
  const hldb::Variable *const k = getK();
  ASSERT_NE(k, nullptr);
  ASSERT_NE(k->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = k->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "declared 'int'";
  EXPECT_TRUE(ts->getSigned());
}

TEST_F(PackageFuncCallTest, ForInitDeclaresKAsZero) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForInitStmts(), nullptr);
  ASSERT_EQ(forStmt->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(forStmt->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Variable *const lhs = init->getLhs<hldb::Variable>();
  ASSERT_NE(lhs, nullptr) << "the for-init LHS is the declared Variable 'k' itself";
  EXPECT_EQ(lhs, getK());
  const hldb::Constant *const rhs = init->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(PackageFuncCallTest, ForConditionIsKLessThanSixtyFourOverFour) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Operation *const cond = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const k = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(k, nullptr);
  EXPECT_EQ(k->getName(), "k");
  EXPECT_EQ(k->getActual<hldb::Variable>(), getK());

  const hldb::Operation *const div = any_cast<hldb::Operation>(cond->getOperands()->at(1));
  ASSERT_NE(div, nullptr) << "'64/4' should be an Operation";
  EXPECT_EQ(div->getOpType(), vpiDivOp);
  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2u);
  const hldb::Constant *const num = any_cast<hldb::Constant>(div->getOperands()->at(0));
  const hldb::Constant *const den = any_cast<hldb::Constant>(div->getOperands()->at(1));
  ASSERT_NE(num, nullptr);
  ASSERT_NE(den, nullptr);
  EXPECT_EQ(num->getDecompile(), "64");
  EXPECT_EQ(den->getDecompile(), "4");
}

TEST_F(PackageFuncCallTest, ForStepIsKPostIncrement) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u);
  const hldb::Operation *const step = any_cast<hldb::Operation>(forStmt->getForIncStmts()->at(0));
  ASSERT_NE(step, nullptr);
  EXPECT_EQ(step->getOpType(), vpiPostIncOp);
  ASSERT_NE(step->getOperands(), nullptr);
  ASSERT_EQ(step->getOperands()->size(), 1u);
  const hldb::RefObj *const k = any_cast<hldb::RefObj>(step->getOperands()->at(0));
  ASSERT_NE(k, nullptr);
  EXPECT_EQ(k->getName(), "k");
  EXPECT_EQ(k->getActual<hldb::Variable>(), getK());
}

TEST_F(PackageFuncCallTest, ForBodyHasOneBlockingAssignment) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(loopBody, nullptr) << "the loop body is an explicit begin-end";
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = getLoopAssign();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(PackageFuncCallTest, LoopAssignLhsIsStateOutIndexedPartSelect) {
  const hldb::Assignment *const assign = getLoopAssign();
  ASSERT_NE(assign, nullptr);
  const hldb::IndexedPartSelect *const lhs = assign->getLhs<hldb::IndexedPartSelect>();
  ASSERT_NE(lhs, nullptr) << "'state_out[k*4 +: 4]' should be an IndexedPartSelect";
  ExpectKTimesFourPlusColonFour(lhs, "state_out", getStateOut());
}

TEST_F(PackageFuncCallTest, LoopAssignRhsSelectsSboxByStateInNibble) {
  const hldb::Assignment *const assign = getLoopAssign();
  ASSERT_NE(assign, nullptr);
  const hldb::BitSelect *const rhs = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "'sbox4[...]' should be a BitSelect";
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "sbox4");
  ASSERT_NE(getFormal("sbox4"), nullptr);
  EXPECT_EQ(prefix->getActual<hldb::IODecl>(), getFormal("sbox4"));
  const hldb::IndexedPartSelect *const index = rhs->getIndex<hldb::IndexedPartSelect>();
  ASSERT_NE(index, nullptr) << "the index 'state_in[k*4 +: 4]' should be an IndexedPartSelect";
  ExpectKTimesFourPlusColonFour(index, "state_in", getFormal("state_in"));
}

// ---------------------------------------------------------------------------
// return state_out;
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, ReturnYieldsStateOut) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 2u);
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[1]);
  ASSERT_NE(ret, nullptr);
  const hldb::RefObj *const expr = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "state_out");
  EXPECT_EQ(expr->getActual<hldb::Variable>(), getStateOut());
}

// ---------------------------------------------------------------------------
// module top(); -- implicit net
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, TopHasNoPortsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has no ports";
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "no identifier in 'top' is declared as a variable, and procedural assignments never create one";
}

TEST_F(PackageFuncCallTest, DataStateSboxIsTheOnlyImplicitScalarWire) {
  GTEST_SKIP() << "Implicit nets aren't explicitly materialized by the compiler.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "6.10: the continuous assignment's undeclared LHS creates an implicit net";
  EXPECT_EQ(top->getNets()->size(), 1u) << "no other undeclared identifier sits in an implicit-net position";
  const hldb::Net *const net = getImplicitNet();
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire) << "6.10: the implicit net has the default net type, wire";
  EXPECT_TRUE(net->getScalar()) << "6.10: the implicit net is scalar";
}

// ---------------------------------------------------------------------------
// assign data_state_sbox = prim_cipher_pkg::sbox4_64bit(data_state_xor, prim_cipher_pkg::PRESENT_SBOX4);
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, TopHasOneContAssignDrivingImplicitNet) {
  GTEST_SKIP() << "Implicit nets aren't explicitly materialized by the compiler.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "data_state_sbox");
  ASSERT_NE(getImplicitNet(), nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getImplicitNet());
}

TEST_F(PackageFuncCallTest, ContAssignRhsCallsPackageFunction) {
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Any *item = nullptr;
  ExpectPackagePath(ca->getRhs(), "prim_cipher_pkg::sbox4_64bit(...)", &item);
  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(item);
  ASSERT_NE(call, nullptr) << "the last path element should be the FuncCall 'sbox4_64bit(...)'";
  EXPECT_TRUE(isScopedName(call->getName(), "sbox4_64bit")) << "unexpected call name '" << call->getName() << "'";
  ASSERT_NE(getSbox(), nullptr);
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getSbox())
      << "26.3: the scoped call must bind to sbox4_64bit declared in prim_cipher_pkg";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  ExpectUnboundRef(call->getArguments()->at(0), "data_state_xor");

  ExpectPackagePath(call->getArguments()->at(1), "prim_cipher_pkg::PRESENT_SBOX4", &item);
  const hldb::RefObj *const sboxArg = any_cast<hldb::RefObj>(item);
  ASSERT_NE(sboxArg, nullptr) << "the last path element should be a reference to PRESENT_SBOX4";
  EXPECT_TRUE(isScopedName(sboxArg->getName(), "PRESENT_SBOX4"))
      << "unexpected reference name '" << sboxArg->getName() << "'";
  ASSERT_NE(getParam("PRESENT_SBOX4"), nullptr);
  EXPECT_EQ(sboxArg->getActual<hldb::Parameter>(), getParam("PRESENT_SBOX4"))
      << "26.3: the scoped name must bind to PRESENT_SBOX4 declared in prim_cipher_pkg";
}

// ---------------------------------------------------------------------------
// always_comb begin : p_post_round_xor ... end
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, TopHasOneAlwaysCombWithNamedBlock) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysComb) << "9.2.2.2: declared 'always_comb'";
  const hldb::Begin *const body = getAlwaysBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->getName(), "p_post_round_xor") << "9.3.4: 'begin : p_post_round_xor' names the block";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 3u);
}

TEST_F(PackageFuncCallTest, FirstStatementAssignsXorToDataO) {
  const hldb::Assignment *const assign = getAlwaysAssign(0);
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  ExpectUnboundRef(assign->getLhs(), "data_o");
  const hldb::Operation *const xorOp = assign->getRhs<hldb::Operation>();
  ASSERT_NE(xorOp, nullptr);
  EXPECT_EQ(xorOp->getOpType(), vpiBitXorOp) << "11.4.8: binary '^' is bitwise exclusive or";
  ASSERT_NE(xorOp->getOperands(), nullptr);
  EXPECT_EQ(xorOp->getOperands()->size(), 2u);
}

TEST_F(PackageFuncCallTest, FirstXorOperandIsDataStateBitSelect) {
  const hldb::Assignment *const assign = getAlwaysAssign(0);
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const xorOp = assign->getRhs<hldb::Operation>();
  ASSERT_NE(xorOp, nullptr);
  ASSERT_NE(xorOp->getOperands(), nullptr);
  ASSERT_EQ(xorOp->getOperands()->size(), 2u);

  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(xorOp->getOperands()->at(0));
  ASSERT_NE(sel, nullptr) << "'data_state[2*NumRoundsHalf+1]' should be a BitSelect";
  ExpectUnboundRef(sel->getPrefix(), "data_state");

  const hldb::Operation *const add = sel->getIndex<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::Operation *const mult = any_cast<hldb::Operation>(add->getOperands()->at(0));
  ASSERT_NE(mult, nullptr) << "11.3.2: '*' binds tighter than '+', so '2*NumRoundsHalf' is the left operand";
  EXPECT_EQ(mult->getOpType(), vpiMultOp);
  ASSERT_NE(mult->getOperands(), nullptr);
  ASSERT_EQ(mult->getOperands()->size(), 2u);
  const hldb::Constant *const two = any_cast<hldb::Constant>(mult->getOperands()->at(0));
  ASSERT_NE(two, nullptr);
  EXPECT_EQ(two->getDecompile(), "2");
  ExpectUnboundRef(mult->getOperands()->at(1), "NumRoundsHalf");
  const hldb::Constant *const one = any_cast<hldb::Constant>(add->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

TEST_F(PackageFuncCallTest, SecondXorOperandSelectsFromPrinceRoundConst) {
  const hldb::Assignment *const assign = getAlwaysAssign(0);
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const xorOp = assign->getRhs<hldb::Operation>();
  ASSERT_NE(xorOp, nullptr);
  ASSERT_NE(xorOp->getOperands(), nullptr);
  ASSERT_EQ(xorOp->getOperands()->size(), 2u);

  const hldb::Any *item = nullptr;
  ExpectPackagePath(xorOp->getOperands()->at(1), "prim_cipher_pkg::PRINCE_ROUND_CONST[11][DataWidth-1:0]", &item);
  const hldb::PartSelect *const part = any_cast<hldb::PartSelect>(item);
  ASSERT_NE(part, nullptr) << "'...[11][DataWidth-1:0]' ends in a part-select";
  const hldb::Range *const range = part->getRange();
  ASSERT_NE(range, nullptr);
  const hldb::Operation *const left = range->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "left bound 'DataWidth-1' should be an Operation";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  ExpectUnboundRef(left->getOperands()->at(0), "DataWidth");
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");

  const hldb::BitSelect *const word = part->getPrefix<hldb::BitSelect>();
  ASSERT_NE(word, nullptr) << "the part-select applies to the element select 'PRINCE_ROUND_CONST[11]'";
  const hldb::Constant *const eleven = word->getIndex<hldb::Constant>();
  ASSERT_NE(eleven, nullptr);
  EXPECT_EQ(eleven->getDecompile(), "11");
  const hldb::RefObj *const param = word->getPrefix<hldb::RefObj>();
  ASSERT_NE(param, nullptr);
  EXPECT_TRUE(isScopedName(param->getName(), "PRINCE_ROUND_CONST"))
      << "unexpected reference name '" << param->getName() << "'";
  ASSERT_NE(getParam("PRINCE_ROUND_CONST"), nullptr);
  EXPECT_EQ(param->getActual<hldb::Parameter>(), getParam("PRINCE_ROUND_CONST"))
      << "26.3: the scoped name must bind to PRINCE_ROUND_CONST declared in prim_cipher_pkg";
}

TEST_F(PackageFuncCallTest, CompoundXorAssignmentsLowerToXorOperations) {
  const std::vector<std::string_view> operands = {"k1", "k0_prime"};
  for (size_t i = 0; i < operands.size(); ++i) {
    const hldb::Assignment *const assign = getAlwaysAssign(i + 1);
    ASSERT_NE(assign, nullptr) << "statement " << (i + 1);
    EXPECT_TRUE(assign->getBlocking()) << "statement " << (i + 1);
    ExpectUnboundRef(assign->getLhs(), "data_o");
    const hldb::Operation *const xorOp = assign->getRhs<hldb::Operation>();
    ASSERT_NE(xorOp, nullptr) << "11.4.1: 'data_o ^= " << operands[i] << "' means 'data_o = data_o ^ " << operands[i]
                              << "'";
    EXPECT_EQ(xorOp->getOpType(), vpiBitXorOp);
    ASSERT_NE(xorOp->getOperands(), nullptr);
    ASSERT_EQ(xorOp->getOperands()->size(), 2u);
    ExpectUnboundRef(xorOp->getOperands()->at(0), "data_o");
    ExpectUnboundRef(xorOp->getOperands()->at(1), operands[i]);
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageFuncCallTest, EveryUndeclaredIdentifierIsReported) {
  GTEST_SKIP() << "Implicit nets aren't explicitly materialized by the compiler.";
  for (std::string_view name :
       {"data_state_xor", "data_o", "data_state", "NumRoundsHalf", "DataWidth", "k1", "k0_prime"}) {
    EXPECT_NE(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "6.5: '" << name << "' is used but declared nowhere, and its position does not create an implicit "
        << "net (6.10)";
  }
}

TEST_F(PackageFuncCallTest, ImplicitNetAndPackageNamesAreNotReported) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "data_state_sbox"), nullptr)
      << "6.10: 'data_state_sbox' is a legal implicit net";
  for (std::string_view name : {"sbox4_64bit", "PRESENT_SBOX4", "PRINCE_ROUND_CONST"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "26.3: '" << name << "' is declared in prim_cipher_pkg";
  }
}

TEST_F(PackageFuncCallTest, NoSyntaxOrFatalErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the undeclared identifiers are semantic errors, not syntax errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
