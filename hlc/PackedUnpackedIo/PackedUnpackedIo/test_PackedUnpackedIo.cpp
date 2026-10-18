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

// Validates the HLDB model built for tests/PackedUnpackedIo/dut.sv:
//
//   package hmac_pkg;
//   typedef logic [31:0] sha_word_t;
//   function automatic sha_word_t [7:0]
//                              compress(
//                                                 input sha_word_t w,
//                                                 input sha_word_t ww [16:0],
//                                                 input sha_word_t [7:0] h_i,
//                                                 input sha_word_t [7:0] hh_ii [16:0],
//                                                 input logic hh [16:0],
//                                                 input logic [7:0] ii [16:0]);
//   endfunction
//   endpackage
//
// The point of the fixture is subroutine formals that combine packed
// dimensions (written before the name) and unpacked dimensions (written after
// it) on a typedef and on 'logic' (IEEE 1800-2023 7.4.1, 7.4.2). The
// regression this file exists to catch is HLC confusing the two kinds of
// dimension, or their order.
//
// Each formal's type is checked as a dimension list, outermost first. An
// unpacked dimension varies more slowly than a packed one (7.4.4), so the
// unpacked dimensions come first, then the packed ones, then the element
// type. For example 'sha_word_t [7:0] hh_ii [16:0]' is
// "unpacked[16:0] packed[7:0] sha_word_t".
//
// What is checked, and why:
//   typedef logic [31:0] sha_word_t; (6.18)
//     - the package declares exactly 1 Typedef, an alias of logic [31:0]
//   function automatic sha_word_t [7:0] compress(...); (13.4)
//     - the package declares exactly 1 subroutine, the automatic (13.4.2)
//       Function compress, whose return type is "packed[7:0] sha_word_t"
//     - exactly 6 formals, in source order, each an input:
//         w      sha_word_t
//         ww     unpacked[16:0] sha_word_t
//         h_i    packed[7:0] sha_word_t
//         hh_ii  unpacked[16:0] packed[7:0] sha_word_t
//         hh     unpacked[16:0] logic
//         ii     unpacked[16:0] packed[7:0] logic
//     - the body is empty: the function has no statement
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics. A
//       function with no return statement returns its implicit variable
//       (13.4.1), so an empty body is legal
//
// Reduction and elaboration: the ranges are constant literals and there is no
// hierarchy, so no check is gated on getElaborated().
//
// What is NOT checked, and why:
//   - Whether HLC records a formal's unpacked dimensions on its typespec or
//     on the declaration's own ranges is a model convention; the dimension
//     list is built from both.
//   - How a RefTypespec refers to sha_word_t: it may resolve to the
//     TypedefTypespec or to the LogicTypespec it aliases. Both are that type
//     (6.18), so either is read as "sha_word_t".
//   - Calling compress, and the value it returns, only exist while simulation
//     runs. Permanently out of scope.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; hmac_pkg is looked up by
//     name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/package.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

#include <string>
#include <string_view>

namespace hlc {

class PackedUnpackedIoTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedUnpackedIo.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("hmac_pkg", m_design->getAllPackages());
  }

  static const hldb::Typedef *getShaWordT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("sha_word_t", pkg->getTypedefs());
  }

  static const hldb::Function *getCompress() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("compress", pkg->getTaskFuncs());
  }

  static std::string rangeText(const hldb::Range *range) {
    if (range == nullptr) return "[?]";
    const hldb::Constant *const l = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = range->getRightExpr<hldb::Constant>();
    return "[" + std::string(l ? l->getDecompile() : "?") + ":" + std::string(r ? r->getDecompile() : "?") + "]";
  }

  // Appends to 'text' the dimensions and element type of 'rts', outermost
  // first: "unpacked[l:r] " or "packed[l:r] " per dimension, then
  // "sha_word_t" or "logic".
  static void describeType(const hldb::RefTypespec *rts, std::string *text) {
    const hldb::Typedef *const sha = getShaWordT();
    while (rts != nullptr) {
      const hldb::Typespec *const actual = rts->getActual();
      if (actual == nullptr) {
        *text += "<unresolved>";
        return;
      }
      const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
      if (((viaTypedef != nullptr) && (viaTypedef->getTypedef() == sha)) ||
          ((sha != nullptr) && (sha->getAlias() != nullptr) && (actual == sha->getAlias()->getActual()))) {
        *text += "sha_word_t";
        return;
      }
      if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(actual)) {
        *text += (at->getPacked() ? "packed" : "unpacked") + rangeText(at->getRange()) + " ";
        rts = at->getElemTypespec();
        continue;
      }
      if (const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(actual)) {
        if (lt->getRanges() != nullptr) {
          for (const hldb::Range *const r : *lt->getRanges()) *text += "packed" + rangeText(r) + " ";
        }
        *text += "logic";
        return;
      }
      *text += "<other>";
      return;
    }
    *text += "<none>";
  }

  // The dimension list of a formal: its own unpacked ranges, if HLC records
  // them on the declaration, then its typespec.
  static std::string describeFormal(const hldb::IODecl *io) {
    std::string text;
    if (io->getRanges() != nullptr) {
      for (const hldb::Range *const r : *io->getRanges()) text += "unpacked" + rangeText(r) + " ";
    }
    describeType(io->getTypespec(), &text);
    return text;
  }
};

TEST_F(PackedUnpackedIoTest, ShaWordTIsLogic31To0) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'hmac_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'sha_word_t' is the package's only typedef";
  const hldb::Typedef *const td = getShaWordT();
  ASSERT_NE(td, nullptr) << "typedef 'sha_word_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::LogicTypespec *const lt = td->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.18: 'sha_word_t' names 'logic [31:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  EXPECT_EQ(rangeText(lt->getRanges()->at(0)), "[31:0]");
}

TEST_F(PackedUnpackedIoTest, CompressIsAutomaticWithEmptyBody) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u) << "'compress' is the package's only subroutine";
  const hldb::Function *const fn = getCompress();
  ASSERT_NE(fn, nullptr) << "function 'compress' not found";
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: declared 'function automatic'";
  EXPECT_EQ(fn->getStmt(), nullptr) << "nothing is written between the header and 'endfunction'";
}

TEST_F(PackedUnpackedIoTest, CompressReturnsPackedArrayOfShaWordT) {
  const hldb::Function *const fn = getCompress();
  ASSERT_NE(fn, nullptr);
  std::string text;
  describeType(fn->getReturn(), &text);
  EXPECT_EQ(text, "packed[7:0] sha_word_t") << "7.4.1: 'sha_word_t [7:0]' is a packed array of sha_word_t";
}

TEST_F(PackedUnpackedIoTest, CompressHasSixInputFormalsInOrder) {
  const hldb::Function *const fn = getCompress();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 6u);
  const char *const names[] = {"w", "ww", "h_i", "hh_ii", "hh", "ii"};
  for (size_t i = 0; i < 6; ++i) {
    const hldb::IODecl *const io = fn->getIODecls()->at(i);
    ASSERT_NE(io, nullptr);
    EXPECT_EQ(io->getName(), names[i]) << "formal " << i << ", in source order";
    EXPECT_EQ(io->getDirection(), vpiInput) << "'" << names[i] << "' is declared 'input'";
  }
}

TEST_F(PackedUnpackedIoTest, FormalDimensionsArePackedOrUnpackedAsWritten) {
  const hldb::Function *const fn = getCompress();
  ASSERT_NE(fn, nullptr);
  struct FormalShape final {
    std::string_view m_name;
    std::string_view m_type;
  };
  const FormalShape shapes[] = {{"w", "sha_word_t"},
                                {"ww", "unpacked[16:0] sha_word_t"},
                                {"h_i", "packed[7:0] sha_word_t"},
                                {"hh_ii", "unpacked[16:0] packed[7:0] sha_word_t"},
                                {"hh", "unpacked[16:0] logic"},
                                {"ii", "unpacked[16:0] packed[7:0] logic"}};
  for (const FormalShape &shape : shapes) {
    const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>(shape.m_name, fn->getIODecls());
    ASSERT_NE(io, nullptr) << "formal '" << shape.m_name << "' not found";
    EXPECT_EQ(describeFormal(io), shape.m_type)
        << "7.4.1, 7.4.2: dimensions before '" << shape.m_name << "' are packed, dimensions after it are unpacked";
  }
}

TEST_F(PackedUnpackedIoTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
