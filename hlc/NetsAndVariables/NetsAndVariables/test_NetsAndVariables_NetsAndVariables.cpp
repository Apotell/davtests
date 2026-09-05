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

// Tests for netvarcomplete/dut.sv (tags: 6.7, 6.8, 23.2.2.3)
//
// Five modules exercise every net-vs-variable permutation described in
// IEEE 1800-2023 Sec 6.7 (nets) / 6.8 (variables) / 23.2.2 (ANSI and
// non-ANSI port declarations):
//   - ansi_ports_net_and_var : ANSI net-kind and variable-kind ports,
//     including bare-identifier port-list inheritance (Sec 23.2.2.3)
//   - nonansi_ports_typed    : non-ANSI ports with the type given directly
//     on the port_declaration
//   - nonansi_ports_companion: non-ANSI ports with a separate "companion"
//     declaration in the module body resolving the net/variable kind
//   - all_net_types          : every net_type keyword, plus vector/signed
//     permutations
//   - all_var_types          : every variable data type, plus explicit
//     "var" keyword permutations
//
// A data type that cannot be a net (reg, integer, real, string, ...) forces
// a Variable even without an explicit "var" keyword (Sec 6.8). A bare
// identifier in an ANSI port list with no header at all inherits the
// direction/kind/type of the immediately preceding port in the same list
// (Sec 23.2.2.3).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <iostream>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_typespec.h>
#include <hldb/byte_typespec.h>
#include <hldb/chandle_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/event_typespec.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/long_int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/short_int_typespec.h>
#include <hldb/short_real_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/time_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NetsAndVariablesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetsAndVariables.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
};

namespace {

// Extracts the integer value of a literal Constant expression. Used for
// range bounds: in this non-elaborated (flat) model, Range::getSize() is
// not computed (it requires evaluating msb-lsb+1, an elaboration-time
// step), but the raw left/right bound expressions parsed straight from the
// source ARE populated as Constant literals -- so range checks here
// validate those literal bound values instead of the (always-zero) size.
// Only handles plain non-negative decimal digit strings, which is all this
// dut.sv uses for range bounds.
int32_t ConstantIntValue(const hldb::Expr *expr) {
  const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
  if (c == nullptr) return -1;
  int32_t result = 0;
  for (const char ch : c->getValue()) {
    if (ch < '0' || ch > '9') return -1;
    result = (result * 10) + (ch - '0');
  }
  return result;
}

// Checks a single Range's left/right bound literals against expected
// values, or that no range exists when expectedLeft < 0 (scalar).
void CheckRangeBounds(const hldb::Typespec *actual, int32_t expectedLeft, int32_t expectedRight, const char *label) {
  const hldb::RangeCollection *const ranges = hldb::getRanges(actual);
  if (expectedLeft >= 0) {
    ASSERT_NE(ranges, nullptr) << label << " expected a vector range";
    ASSERT_EQ(ranges->size(), 1u);
    const hldb::Range *const r = ranges->at(0);
    ASSERT_NE(r->getLeftExpr(), nullptr) << label << " range has no left bound expression";
    ASSERT_NE(r->getRightExpr(), nullptr) << label << " range has no right bound expression";
    EXPECT_EQ(ConstantIntValue(r->getLeftExpr()), expectedLeft) << label << " unexpected range left bound";
    EXPECT_EQ(ConstantIntValue(r->getRightExpr()), expectedRight) << label << " unexpected range right bound";
  } else {
    EXPECT_TRUE(ranges == nullptr || ranges->empty()) << label << " expected scalar (no range)";
  }
}

// Expected shape of a net-kind declaration (and, if it is also a port, of
// the port that carries it).
struct NetExpectation {
  const char *name;
  int32_t netType;
  int32_t rangeLeft;   // -1 => scalar (no range expected)
  int32_t rangeRight;  // meaningful only when rangeLeft >= 0
  int32_t isSigned;    // -1 => not checked, 0 => false, 1 => true
  int32_t direction;   // -1 => not a port, else vpiInput/vpiOutput/vpiInout
  int32_t isScalar;    // -1 => not checked, 0 => false, 1 => true (net's own vpiScalar flag)
  int32_t isVector;    // -1 => not checked, 0 => false, 1 => true (net's own vpiVector flag)
};

// Expected shape of a variable-kind declaration (and, if it is also a port,
// of the port that carries it).
struct VarExpectation {
  const char *name;
  hldb::AnyType typeKind;
  int32_t isSigned;    // -1 => not checked, 0 => false, 1 => true
  int32_t rangeLeft;   // -1 => scalar / not applicable
  int32_t rangeRight;  // meaningful only when rangeLeft >= 0
  int32_t direction;   // -1 => not a port, else vpiInput/vpiOutput/vpiInout
};

void CheckNet(const hldb::Module *mod, const NetExpectation &e) {
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>(e.name, mod->getNets());
  ASSERT_NE(net, nullptr) << "net '" << e.name << "' not found";
  EXPECT_EQ(net->getNetType(), e.netType) << "net '" << e.name << "' unexpected netType";

  ASSERT_NE(net->getTypespec(), nullptr) << "net '" << e.name << "' has no typespec";
  const hldb::Typespec *const actual = net->getTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "net '" << e.name << "' typespec has no actual";

  if (e.isSigned >= 0) {
    EXPECT_EQ(hldb::getSigned(actual), static_cast<bool>(e.isSigned)) << "net '" << e.name << "' unexpected signedness";
  }

  if (e.isScalar >= 0) {
    EXPECT_EQ(net->getScalar(), static_cast<bool>(e.isScalar)) << "net '" << e.name << "' unexpected scalar flag";
  }
  if (e.isVector >= 0) {
    EXPECT_EQ(net->getVector(), static_cast<bool>(e.isVector)) << "net '" << e.name << "' unexpected vector flag";
  }

  const std::string label = std::string("net '") + e.name + "'";
  CheckRangeBounds(actual, e.rangeLeft, e.rangeRight, label.c_str());

  if (e.direction >= 0) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(e.name, mod->getPorts());
    ASSERT_NE(port, nullptr) << "port '" << e.name << "' not found";
    EXPECT_EQ(port->getDirection(), e.direction) << "port '" << e.name << "' unexpected direction";
    const hldb::RefObj *const lc = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lc, nullptr) << "port '" << e.name << "' has no lowConn RefObj";
    const hldb::Net *const resolved = lc->getActual<hldb::Net>();
    ASSERT_NE(resolved, nullptr) << "port '" << e.name << "' lowConn does not resolve to a Net";
    EXPECT_EQ(resolved->getName(), e.name);
  }
}

// hldb::getReferenced<R>() (Utils.h) cannot be used here: its PackageTypespec
// switch case returns a bare Package* without casting to R, which fails to
// compile for any R other than Package (every switch case is instantiated
// regardless of the runtime type). See hldb_model_gaps.md. These two local
// helpers replicate only the TypedefTypespec-unwrapping this file needs.
const hldb::StructTypespec *ResolveStruct(const hldb::Typespec *actual) {
  if (const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(actual)) return st;
  if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(actual)) {
    if (tdt->getTypedefAlias() != nullptr) return tdt->getTypedefAlias()->getActual<hldb::StructTypespec>();
  }
  return nullptr;
}

void CheckVar(const hldb::Module *mod, const VarExpectation &e) {
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>(e.name, mod->getVariables());
  ASSERT_NE(var, nullptr) << "variable '" << e.name << "' not found";

  ASSERT_NE(var->getTypespec(), nullptr) << "variable '" << e.name << "' has no typespec";
  const hldb::Typespec *const actual = var->getTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "variable '" << e.name << "' typespec has no actual";
  // Compared via the underlying integer, not the enum directly: hldb::AnyType
  // has no exported operator<< in this install (AnyTypeName() is missing the
  // HLDB_API export macro in third_party/hldb/templates/any_type.h -- see
  // hldb_model_gaps.md), so GTest's failure-message printer would fail to
  // link if the raw enum were streamed.
  EXPECT_EQ(static_cast<uint32_t>(actual->getAnyType()), static_cast<uint32_t>(e.typeKind))
      << "variable '" << e.name << "' unexpected typespec kind";

  if (e.isSigned >= 0) {
    EXPECT_EQ(hldb::getSigned(actual), static_cast<bool>(e.isSigned))
        << "variable '" << e.name << "' unexpected signedness";
  }

  if (e.rangeLeft >= 0 || e.typeKind == hldb::AnyType::LogicTypespec || e.typeKind == hldb::AnyType::BitTypespec) {
    const std::string label = std::string("variable '") + e.name + "'";
    CheckRangeBounds(actual, e.rangeLeft, e.rangeRight, label.c_str());
  }

  if (e.direction >= 0) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(e.name, mod->getPorts());
    ASSERT_NE(port, nullptr) << "port '" << e.name << "' not found";
    EXPECT_EQ(port->getDirection(), e.direction) << "port '" << e.name << "' unexpected direction";
    const hldb::RefObj *const lc = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lc, nullptr) << "port '" << e.name << "' has no lowConn RefObj";
    const hldb::Variable *const resolved = lc->getActual<hldb::Variable>();
    ASSERT_NE(resolved, nullptr) << "port '" << e.name << "' lowConn does not resolve to a Variable";
    EXPECT_EQ(resolved->getName(), e.name);
  }
}

// Prints the presence/absence of every name in "names" against the module's
// getNets() collection, one line per name, and fails the test (listing each
// missing name individually) if any are not found. Unlike CheckNet() above,
// this does not verify shape (netType/width/signedness/port) -- it is purely
// an existence report so a missing declaration is immediately visible in the
// test output instead of being buried inside a shape-mismatch message.
void ReportNetPresence(const hldb::Module *mod, const char *moduleLabel, std::initializer_list<const char *> names) {
  std::cout << "[NETS] module '" << moduleLabel << "': expected " << names.size() << "\n";
  for (const char *const name : names) {
    const hldb::Net *const net = (mod != nullptr) ? hldb::findByName<hldb::Net>(name, mod->getNets()) : nullptr;
    const hldb::Variable *const asVar =
        (mod != nullptr) ? hldb::findByName<hldb::Variable>(name, mod->getVariables()) : nullptr;
    if (net != nullptr) {
      std::cout << "  FOUND   net '" << name << "'";
      if (asVar != nullptr) {
        // Present in BOTH collections under the same name -- a duplicate /
        // ambiguous classification that a plain "FOUND" would silently mask.
        std::cout << " (WARNING: ALSO present as a Variable -- ambiguous)";
        ADD_FAILURE() << "'" << name << "' in module '" << moduleLabel
                       << "' is present in BOTH Nets and Variables -- ambiguous classification";
      }
      std::cout << "\n";
    } else if (asVar != nullptr) {
      // Not in getNets(), but present in getVariables() -- distinguishes a
      // misclassified declaration from one genuinely absent from both.
      std::cout << "  MISSING net '" << name << "' (found instead as a Variable)\n";
      ADD_FAILURE() << "'" << name << "' in module '" << moduleLabel
                     << "' is modeled as a Variable, not a Net as expected";
    } else {
      // Absent from both Nets and Variables -- check whether the Port
      // itself still exists, to distinguish "the whole declaration was
      // dropped" from "the port survived but its backing net/variable
      // didn't get created."
      const hldb::Port *const port = (mod != nullptr) ? hldb::findByName<hldb::Port>(name, mod->getPorts()) : nullptr;
      if (port != nullptr) {
        std::cout << "  MISSING net '" << name << "' (a Port with this name exists, but no backing Net/Variable)\n";
        ADD_FAILURE() << "port '" << name << "' in module '" << moduleLabel
                       << "' exists but has no backing Net (nor Variable)";
      } else {
        std::cout << "  MISSING net '" << name << "' (not found in Nets, Variables, or Ports)\n";
        ADD_FAILURE() << "net '" << name << "' not found in module '" << moduleLabel << "'";
      }
    }
  }
}

// Same as ReportNetPresence(), but against the module's getVariables()
// collection.
void ReportVarPresence(const hldb::Module *mod, const char *moduleLabel, std::initializer_list<const char *> names) {
  std::cout << "[VARS] module '" << moduleLabel << "': expected " << names.size() << "\n";
  for (const char *const name : names) {
    const hldb::Variable *const var = (mod != nullptr) ? hldb::findByName<hldb::Variable>(name, mod->getVariables()) : nullptr;
    const hldb::Net *const asNet = (mod != nullptr) ? hldb::findByName<hldb::Net>(name, mod->getNets()) : nullptr;
    if (var != nullptr) {
      std::cout << "  FOUND   variable '" << name << "'";
      if (asNet != nullptr) {
        std::cout << " (WARNING: ALSO present as a Net -- ambiguous)";
        ADD_FAILURE() << "'" << name << "' in module '" << moduleLabel
                       << "' is present in BOTH Nets and Variables -- ambiguous classification";
      }
      std::cout << "\n";
    } else if (asNet != nullptr) {
      std::cout << "  MISSING variable '" << name << "' (found instead as a Net)\n";
      ADD_FAILURE() << "'" << name << "' in module '" << moduleLabel
                     << "' is modeled as a Net, not a Variable as expected";
    } else {
      const hldb::Port *const port = (mod != nullptr) ? hldb::findByName<hldb::Port>(name, mod->getPorts()) : nullptr;
      if (port != nullptr) {
        std::cout << "  MISSING variable '" << name << "' (a Port with this name exists, but no backing Net/Variable)\n";
        ADD_FAILURE() << "port '" << name << "' in module '" << moduleLabel
                       << "' exists but has no backing Variable (nor Net)";
      } else {
        std::cout << "  MISSING variable '" << name << "' (not found in Nets, Variables, or Ports)\n";
        ADD_FAILURE() << "variable '" << name << "' not found in module '" << moduleLabel << "'";
      }
    }
  }
}

// Expected typespec-actual resolution for a single net or variable
// declaration. "hasExplicitType" records whether the SV source spells out
// an explicit data type keyword (e.g. "logic", "reg", "integer", a typedef
// name, ...) for this declaration -- either directly, or inherited via a
// bare identifier from a preceding port header -- as opposed to only a
// net_type/var keyword (or nothing at all), which leaves the data type
// implicit (defaulting to 1-bit logic, Sec 6.7/6.8/23.2.2.3).
struct TypespecActualExpectation {
  const char *name;
  bool hasExplicitType;
};

// Checks that a net's RefTypespec::getActual() is non-null exactly when the
// source declares an explicit data type, and null exactly when it doesn't.
// This is purely a presence/absence check on "actual" -- CheckNet() above
// already covers netType/width/signedness/port shape once "actual" exists.
// Uses EXPECT_ (not ASSERT_) throughout so every declaration in a table is
// checked and reported even if an earlier one mismatches.
void CheckNetTypespecActual(const hldb::Module *mod, const char *moduleLabel, const TypespecActualExpectation &e) {
  const hldb::Net *const net = (mod != nullptr) ? hldb::findByName<hldb::Net>(e.name, mod->getNets()) : nullptr;
  if (net == nullptr) {
    std::cout << "  SKIP  net '" << e.name << "' in '" << moduleLabel << "' -- not found as a Net (see presence report)\n";
    return;
  }
  if (net->getTypespec() == nullptr) {
    std::cout << "  ISSUE net '" << e.name << "' in '" << moduleLabel << "' has no RefTypespec at all\n";
    ADD_FAILURE() << "net '" << e.name << "' in module '" << moduleLabel << "' has no RefTypespec";
    return;
  }
  const hldb::Typespec *const actual = net->getTypespec()->getActual();
  if (e.hasExplicitType) {
    if (actual != nullptr) {
      std::cout << "  OK    net '" << e.name << "' has a non-null actual, as expected (explicit type in source)\n";
    } else {
      std::cout << "  ISSUE net '" << e.name
                 << "' has a NULL actual, but the source declares an explicit data type -- expected non-null\n";
      ADD_FAILURE() << "net '" << e.name << "' in module '" << moduleLabel
                     << "': expected a non-null typespec actual (explicit data type in source), but actual is null";
    }
  } else {
    if (actual == nullptr) {
      std::cout << "  OK    net '" << e.name << "' has a null actual, as expected (no explicit type in source)\n";
    } else {
      std::cout << "  ISSUE net '" << e.name
                 << "' has a NON-NULL actual, but the source has no explicit data type -- expected null\n";
      ADD_FAILURE() << "net '" << e.name << "' in module '" << moduleLabel
                     << "': expected a null typespec actual (no explicit data type in source), but actual is non-null";
    }
  }
}

// Same as CheckNetTypespecActual(), but against the module's getVariables()
// collection.
void CheckVarTypespecActual(const hldb::Module *mod, const char *moduleLabel, const TypespecActualExpectation &e) {
  const hldb::Variable *const var = (mod != nullptr) ? hldb::findByName<hldb::Variable>(e.name, mod->getVariables()) : nullptr;
  if (var == nullptr) {
    std::cout << "  SKIP  variable '" << e.name << "' in '" << moduleLabel
               << "' -- not found as a Variable (see presence report)\n";
    return;
  }
  if (var->getTypespec() == nullptr) {
    std::cout << "  ISSUE variable '" << e.name << "' in '" << moduleLabel << "' has no RefTypespec at all\n";
    ADD_FAILURE() << "variable '" << e.name << "' in module '" << moduleLabel << "' has no RefTypespec";
    return;
  }
  const hldb::Typespec *const actual = var->getTypespec()->getActual();
  if (e.hasExplicitType) {
    if (actual != nullptr) {
      std::cout << "  OK    variable '" << e.name << "' has a non-null actual, as expected (explicit type in source)\n";
    } else {
      std::cout << "  ISSUE variable '" << e.name
                 << "' has a NULL actual, but the source declares an explicit data type -- expected non-null\n";
      ADD_FAILURE() << "variable '" << e.name << "' in module '" << moduleLabel
                     << "': expected a non-null typespec actual (explicit data type in source), but actual is null";
    }
  } else {
    if (actual == nullptr) {
      std::cout << "  OK    variable '" << e.name << "' has a null actual, as expected (no explicit type in source)\n";
    } else {
      std::cout << "  ISSUE variable '" << e.name
                 << "' has a NON-NULL actual, but the source has no explicit data type -- expected null\n";
      ADD_FAILURE() << "variable '" << e.name << "' in module '" << moduleLabel
                     << "': expected a null typespec actual (no explicit data type in source), but actual is non-null";
    }
  }
}

}  // namespace

// =====================================================================
// Compiler diagnostics
// =====================================================================

TEST_F(NetsAndVariablesTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// =====================================================================
// Section 1: ansi_ports_net_and_var
// =====================================================================

TEST_F(NetsAndVariablesTest, Section1_ModuleExists) { EXPECT_NE(getModule("ansi_ports_net_and_var"), nullptr); }

TEST_F(NetsAndVariablesTest, Section1_HasTwentyTwoPorts) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 22u);
}

TEST_F(NetsAndVariablesTest, Section1_HasThirteenNets) {
  // "output logic o_logic_default" -- explicit data_type ("logic") on an output
  // port with no port kind given defaults to Variable, not Net (Sec 23.2.2.3),
  // so it counts toward getVariables() below, not here.
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 13u);
}

TEST_F(NetsAndVariablesTest, Section1_HasNineVariables) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 9u);
}

TEST_F(NetsAndVariablesTest, Section1_NetPortsAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  const NetExpectation kNets[] = {
      {"i_wire", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"i_wire_logic", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"i_tri_bus", vpiTri, 3, 0, 0, vpiInput, 0, 1},
      {"i_logic_default", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"i_a", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"i_b", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"o_wire", vpiWire, -1, -1, 0, vpiOutput, 1, 0},
      {"o_wand_logic", vpiWand, -1, -1, 0, vpiOutput, 1, 0},
      {"io_wire", vpiWire, -1, -1, 0, vpiInout, 1, 0},
      {"io_tri", vpiTri, -1, -1, 0, vpiInout, 1, 0},
      {"o_wire1", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"o_wire2", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"o_wire3", vpiWire, -1, -1, 0, vpiInput, 1, 0},
  };
  for (const NetExpectation &e : kNets) {
    CheckNet(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section1_VarPortsAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  const VarExpectation kVars[] = {
      {"i_var_logic", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiInput},
      {"i_var_logic2", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiInput},
      {"o_var_logic", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
      // "output logic o_logic_default" -- explicit data_type with no port kind on an
      // output port defaults to Variable, not Net (Sec 23.2.2.3), despite the lack
      // of an explicit "var" keyword.
      {"o_logic_default", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
      {"o_c", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
      {"o_d", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
      {"o_reg", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
      {"o_integer", hldb::AnyType::IntegerTypespec, 1, -1, -1, vpiOutput},
      {"o_real", hldb::AnyType::RealTypespec, -1, -1, -1, vpiOutput},
  };
  for (const VarExpectation &e : kVars) {
    CheckVar(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section1_BareIdentifierIbInheritsFromIa) {
  // Sec 23.2.2.3: a bare identifier in an ANSI port list with no header
  // inherits direction/kind/type from the immediately preceding port.
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const ia = hldb::findByName<hldb::Net>("i_a", mod->getNets());
  const hldb::Net *const ib = hldb::findByName<hldb::Net>("i_b", mod->getNets());
  ASSERT_NE(ia, nullptr);
  ASSERT_NE(ib, nullptr);
  EXPECT_EQ(ib->getNetType(), ia->getNetType());

  const hldb::Port *const pa = hldb::findByName<hldb::Port>("i_a", mod->getPorts());
  const hldb::Port *const pb = hldb::findByName<hldb::Port>("i_b", mod->getPorts());
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pb, nullptr);
  EXPECT_EQ(pb->getDirection(), pa->getDirection());
}

TEST_F(NetsAndVariablesTest, Section1_BareIdentifierOdInheritsFromOc) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const oc = hldb::findByName<hldb::Variable>("o_c", mod->getVariables());
  const hldb::Variable *const od = hldb::findByName<hldb::Variable>("o_d", mod->getVariables());
  ASSERT_NE(oc, nullptr);
  ASSERT_NE(od, nullptr);

  const hldb::Port *const pc = hldb::findByName<hldb::Port>("o_c", mod->getPorts());
  const hldb::Port *const pd = hldb::findByName<hldb::Port>("o_d", mod->getPorts());
  ASSERT_NE(pc, nullptr);
  ASSERT_NE(pd, nullptr);
  EXPECT_EQ(pd->getDirection(), pc->getDirection());
}

TEST_F(NetsAndVariablesTest, Section1_BareIdentifiersOwire2AndOwire3InheritFromOwire1) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  const hldb::Port *const p1 = hldb::findByName<hldb::Port>("o_wire1", mod->getPorts());
  const hldb::Port *const p2 = hldb::findByName<hldb::Port>("o_wire2", mod->getPorts());
  const hldb::Port *const p3 = hldb::findByName<hldb::Port>("o_wire3", mod->getPorts());
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  ASSERT_NE(p3, nullptr);
  EXPECT_EQ(p2->getDirection(), p1->getDirection());
  EXPECT_EQ(p3->getDirection(), p1->getDirection());

  const hldb::Net *const n1 = hldb::findByName<hldb::Net>("o_wire1", mod->getNets());
  const hldb::Net *const n2 = hldb::findByName<hldb::Net>("o_wire2", mod->getNets());
  const hldb::Net *const n3 = hldb::findByName<hldb::Net>("o_wire3", mod->getNets());
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);
  ASSERT_NE(n3, nullptr);
  EXPECT_EQ(n2->getNetType(), n1->getNetType());
  EXPECT_EQ(n3->getNetType(), n1->getNetType());
}

TEST_F(NetsAndVariablesTest, Section1_HasFiveContAssigns) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 5u);
}

TEST_F(NetsAndVariablesTest, Section1_HasOneAlwaysCombProcess) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Always *const always = any_cast<hldb::Always>(mod->getProcesses()->at(0));
  ASSERT_NE(always, nullptr) << "expected an Always (always_comb) process";
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysComb);
}

TEST_F(NetsAndVariablesTest, Section1_PrintExpectedNetsAndVariables) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  ReportNetPresence(mod, "ansi_ports_net_and_var",
                     {"i_wire", "i_wire_logic", "i_tri_bus", "i_logic_default", "i_a", "i_b", "o_wire",
                      "o_wand_logic", "io_wire", "io_tri", "o_wire1", "o_wire2", "o_wire3"});
  ReportVarPresence(
      mod, "ansi_ports_net_and_var",
      {"i_var_logic", "i_var_logic2", "o_var_logic", "o_logic_default", "o_c", "o_d", "o_reg", "o_integer", "o_real"});
}

TEST_F(NetsAndVariablesTest, Section1_TypespecActualMatchesExplicitType) {
  const hldb::Module *const mod = getModule("ansi_ports_net_and_var");
  ASSERT_NE(mod, nullptr);
  // Every net/variable resolves a non-null typespec actual regardless of whether the
  // source spells out an explicit type keyword: when the source omits it, the compiler
  // synthesizes the IEEE-default (1-bit logic, Sec 6.7/6.8/23.2.2.3) rather than leaving
  // it unresolved. "hasExplicitType" below records what the *source* spells out, purely
  // for documentation -- it is no longer a predictor of null-vs-non-null actual.
  const TypespecActualExpectation kNets[] = {
      {"i_wire", true},          // "input wire i_wire" -- no data type; defaults to logic
      {"i_wire_logic", true},    // "input wire logic i_wire_logic"
      {"i_tri_bus", true},       // "input tri [3:0] i_tri_bus" -- no "logic" keyword, but the
                                 // packed dimension [3:0] still requires a resolved typespec
      {"i_logic_default", true}, // "input logic i_logic_default"
      {"i_a", true},             // "input wire logic i_a"
      {"i_b", true},             // bare -- inherits i_a's "wire logic"
      {"o_wire", true},          // "output wire o_wire" -- no data type; defaults to logic
      {"o_wand_logic", true},    // "output wand logic o_wand_logic"
      {"io_wire", true},         // "inout wire io_wire" -- no data type; defaults to logic
      {"io_tri", true},          // "inout tri io_tri" -- no data type; defaults to logic
      {"o_wire1", true},         // "input wire o_wire1" -- no data type; defaults to logic
      {"o_wire2", true},         // bare -- inherits o_wire1's "wire" (no explicit type)
      {"o_wire3", true},         // bare -- inherits o_wire1's "wire" (no explicit type)
  };
  const TypespecActualExpectation kVars[] = {
      {"i_var_logic", true},      // "input var logic i_var_logic"
      {"i_var_logic2", true},     // bare -- inherits "var logic"
      {"o_var_logic", true},      // "output var logic o_var_logic"
      {"o_logic_default", true},  // "output logic o_logic_default" -- explicit data_type, no
                                  // port kind -> defaults to Variable (Sec 23.2.2.3)
      {"o_c", true},              // "output var logic o_c"
      {"o_d", true},              // bare -- inherits o_c's "var logic"
      {"o_reg", true},            // "output reg o_reg" -- reg is the type
      {"o_integer", true},        // "output integer o_integer"
      {"o_real", true},           // "output real o_real"
  };
  std::cout << "[TYPESPEC-ACTUAL] module 'ansi_ports_net_and_var'\n";
  for (const TypespecActualExpectation &e : kNets) CheckNetTypespecActual(mod, "ansi_ports_net_and_var", e);
  for (const TypespecActualExpectation &e : kVars) CheckVarTypespecActual(mod, "ansi_ports_net_and_var", e);
}

// =====================================================================
// Section 2: nonansi_ports_typed
// =====================================================================

TEST_F(NetsAndVariablesTest, Section2_ModuleExists) { EXPECT_NE(getModule("nonansi_ports_typed"), nullptr); }

TEST_F(NetsAndVariablesTest, Section2_HasFivePorts) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 5u);
}

TEST_F(NetsAndVariablesTest, Section2_HasThreeNets) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 3u);
}

TEST_F(NetsAndVariablesTest, Section2_HasTwoVariables) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 2u);
}

TEST_F(NetsAndVariablesTest, Section2_NetPortsAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  const NetExpectation kNets[] = {
      {"i_wire", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"o_wire", vpiWire, -1, -1, 0, vpiOutput, 1, 0},
      {"io_wire", vpiWire, -1, -1, 0, vpiInout, 1, 0},
  };
  for (const NetExpectation &e : kNets) {
    CheckNet(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section2_VarPortsAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  const VarExpectation kVars[] = {
      {"i_var", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiInput},
      {"o_reg", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
  };
  for (const VarExpectation &e : kVars) {
    CheckVar(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section2_HasTwoContAssigns) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 2u);
}

TEST_F(NetsAndVariablesTest, Section2_HasOneAlwaysCombProcess) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Always *const always = any_cast<hldb::Always>(mod->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysComb);
}

TEST_F(NetsAndVariablesTest, Section2_PrintExpectedNetsAndVariables) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  ReportNetPresence(mod, "nonansi_ports_typed", {"i_wire", "o_wire", "io_wire"});
  ReportVarPresence(mod, "nonansi_ports_typed", {"i_var", "o_reg"});
}

TEST_F(NetsAndVariablesTest, Section2_TypespecActualMatchesExplicitType) {
  const hldb::Module *const mod = getModule("nonansi_ports_typed");
  ASSERT_NE(mod, nullptr);
  // See the comment on Section1_TypespecActualMatchesExplicitType: an omitted data type
  // still resolves a non-null actual (defaults to logic), so every entry here is true.
  const TypespecActualExpectation kNets[] = {
      {"i_wire", true},  // "input wire i_wire;" -- no data type; defaults to logic
      {"o_wire", true},  // "output wire o_wire;" -- no data type; defaults to logic
      {"io_wire", true}, // "inout wire io_wire;" -- no data type; defaults to logic
  };
  const TypespecActualExpectation kVars[] = {
      {"i_var", true},  // "input var logic i_var;"
      {"o_reg", true},  // "output reg o_reg;" -- reg is the type
  };
  std::cout << "[TYPESPEC-ACTUAL] module 'nonansi_ports_typed'\n";
  for (const TypespecActualExpectation &e : kNets) CheckNetTypespecActual(mod, "nonansi_ports_typed", e);
  for (const TypespecActualExpectation &e : kVars) CheckVarTypespecActual(mod, "nonansi_ports_typed", e);
}

// =====================================================================
// Section 3: nonansi_ports_companion
// =====================================================================
//
// Each port here has NO type at all in its own port_declaration; the actual
// net/variable kind comes from a separate "companion" declaration in the
// module body sharing the same identifier (Sec 23.2.2.2). The port and its
// companion must resolve to a single, merged object -- not two distinct
// objects with the same name.

TEST_F(NetsAndVariablesTest, Section3_ModuleExists) { EXPECT_NE(getModule("nonansi_ports_companion"), nullptr); }

TEST_F(NetsAndVariablesTest, Section3_HasFivePorts) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 5u);
}

TEST_F(NetsAndVariablesTest, Section3_HasExactlyThreeNets) {
  // i1, o2, io1 -- if the port's bare declaration and its companion each
  // produced a separate object, this would be higher than 3.
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 3u);
}

TEST_F(NetsAndVariablesTest, Section3_HasExactlyTwoVariables) {
  // i2, o1 -- likewise must not be duplicated by the split declaration.
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 2u);
}

TEST_F(NetsAndVariablesTest, Section3_NetPortsAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  const NetExpectation kNets[] = {
      {"i1", vpiWire, -1, -1, 0, vpiInput, 1, 0},
      {"o2", vpiWire, -1, -1, 0, vpiOutput, 1, 0},
      {"io1", vpiTri, -1, -1, 0, vpiInout, 1, 0},
  };
  for (const NetExpectation &e : kNets) {
    CheckNet(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section3_VarPortsAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  const VarExpectation kVars[] = {
      {"i2", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiInput},
      {"o1", hldb::AnyType::LogicTypespec, 0, -1, -1, vpiOutput},
  };
  for (const VarExpectation &e : kVars) {
    CheckVar(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section3_HasTwoContAssigns) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 2u);
}

TEST_F(NetsAndVariablesTest, Section3_HasOneAlwaysCombProcess) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Always *const always = any_cast<hldb::Always>(mod->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysComb);
}

TEST_F(NetsAndVariablesTest, Section3_PrintExpectedNetsAndVariables) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  ReportNetPresence(mod, "nonansi_ports_companion", {"i1", "o2", "io1"});
  ReportVarPresence(mod, "nonansi_ports_companion", {"i2", "o1"});
}

TEST_F(NetsAndVariablesTest, Section3_TypespecActualMatchesExplicitType) {
  const hldb::Module *const mod = getModule("nonansi_ports_companion");
  ASSERT_NE(mod, nullptr);
  // See the comment on Section1_TypespecActualMatchesExplicitType: an omitted data type
  // still resolves a non-null actual (defaults to logic), so every entry here is true.
  const TypespecActualExpectation kNets[] = {
      {"i1", true},   // companion "wire i1;" -- no data type; defaults to logic
      {"o2", true},   // companion "wire o2;" -- no data type; defaults to logic
      {"io1", true},  // companion "tri io1;" -- no data type; defaults to logic
  };
  const TypespecActualExpectation kVars[] = {
      {"i2", true},  // companion "logic i2;"
      {"o1", true},  // companion "reg o1;" -- reg is the type
  };
  std::cout << "[TYPESPEC-ACTUAL] module 'nonansi_ports_companion'\n";
  for (const TypespecActualExpectation &e : kNets) CheckNetTypespecActual(mod, "nonansi_ports_companion", e);
  for (const TypespecActualExpectation &e : kVars) CheckVarTypespecActual(mod, "nonansi_ports_companion", e);
}

// =====================================================================
// Section 4: all_net_types
// =====================================================================

TEST_F(NetsAndVariablesTest, Section4_ModuleExists) { EXPECT_NE(getModule("all_net_types"), nullptr); }

TEST_F(NetsAndVariablesTest, Section4_HasNoPorts) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getPorts() == nullptr || mod->getPorts()->empty());
}

TEST_F(NetsAndVariablesTest, Section4_HasFifteenNets) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 15u);
}

TEST_F(NetsAndVariablesTest, Section4_HasNoVariables) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getVariables() == nullptr || mod->getVariables()->empty());
}

TEST_F(NetsAndVariablesTest, Section4_EveryNetTypeKeywordIsCorrectlyClassified) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  const NetExpectation kNets[] = {
      {"n_supply0", vpiSupply0, -1, -1, 0, -1, 1, 0},
      {"n_supply1", vpiSupply1, -1, -1, 0, -1, 1, 0},
      {"n_tri", vpiTri, -1, -1, 0, -1, 1, 0},
      {"n_triand", vpiTriAnd, -1, -1, 0, -1, 1, 0},
      {"n_trior", vpiTriOr, -1, -1, 0, -1, 1, 0},
      {"n_trireg", vpiTriReg, -1, -1, 0, -1, 1, 0},
      {"n_tri0", vpiTri0, -1, -1, 0, -1, 1, 0},
      {"n_tri1", vpiTri1, -1, -1, 0, -1, 1, 0},
      {"n_uwire", vpiUwire, -1, -1, 0, -1, 1, 0},
      {"n_wire", vpiWire, -1, -1, 0, -1, 1, 0},
      {"n_wand", vpiWand, -1, -1, 0, -1, 1, 0},
      {"n_wor", vpiWor, -1, -1, 0, -1, 1, 0},
      {"n_wire_bus", vpiWire, 7, 0, 0, -1, 0, 1},
      {"n_wire_logic", vpiWire, -1, -1, 0, -1, 1, 0},
      {"n_tri_signed", vpiTri, 3, 0, 1, -1, 0, 1},
  };
  for (const NetExpectation &e : kNets) {
    CheckNet(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section4_HasTwelveContAssigns) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 12u);
}

TEST_F(NetsAndVariablesTest, Section4_HasNoProcesses) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getProcesses() == nullptr || mod->getProcesses()->empty());
}

TEST_F(NetsAndVariablesTest, Section4_PrintExpectedNets) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  ReportNetPresence(mod, "all_net_types",
                     {"n_supply0", "n_supply1", "n_tri", "n_triand", "n_trior", "n_trireg", "n_tri0", "n_tri1",
                      "n_uwire", "n_wire", "n_wand", "n_wor", "n_wire_bus", "n_wire_logic", "n_tri_signed"});
}

TEST_F(NetsAndVariablesTest, Section4_TypespecActualMatchesExplicitType) {
  const hldb::Module *const mod = getModule("all_net_types");
  ASSERT_NE(mod, nullptr);
  // See the comment on Section1_TypespecActualMatchesExplicitType: an omitted data type
  // still resolves a non-null actual (defaults to logic), so every entry here is true.
  const TypespecActualExpectation kNets[] = {
      {"n_supply0", true},     // "supply0 n_supply0;" -- no data type; defaults to logic
      {"n_supply1", true},     // "supply1 n_supply1;" -- no data type; defaults to logic
      {"n_tri", true},         // "tri n_tri;" -- no data type; defaults to logic
      {"n_triand", true},      // "triand n_triand;" -- no data type; defaults to logic
      {"n_trior", true},       // "trior n_trior;" -- no data type; defaults to logic
      {"n_trireg", true},      // "trireg n_trireg;" -- no data type; defaults to logic
      {"n_tri0", true},        // "tri0 n_tri0;" -- no data type; defaults to logic
      {"n_tri1", true},        // "tri1 n_tri1;" -- no data type; defaults to logic
      {"n_uwire", true},       // "uwire n_uwire;" -- no data type; defaults to logic
      {"n_wire", true},        // "wire n_wire;" -- no data type; defaults to logic
      {"n_wand", true},        // "wand n_wand;" -- no data type; defaults to logic
      {"n_wor", true},         // "wor n_wor;" -- no data type; defaults to logic
      {"n_wire_bus", true},     // "wire [7:0] n_wire_bus;" -- no "logic" keyword, but the packed
                                // dimension [7:0] still requires a resolved typespec
      {"n_wire_logic", true},   // "wire logic n_wire_logic;" -- explicit "logic"
      {"n_tri_signed", true},   // "tri signed [3:0] n_tri_signed;" -- no "logic" keyword, but the
                                // packed dimension [3:0] still requires a resolved typespec
  };
  std::cout << "[TYPESPEC-ACTUAL] module 'all_net_types'\n";
  for (const TypespecActualExpectation &e : kNets) CheckNetTypespecActual(mod, "all_net_types", e);
}

// =====================================================================
// Section 5: all_var_types
// =====================================================================

TEST_F(NetsAndVariablesTest, Section5_ModuleExists) { EXPECT_NE(getModule("all_var_types"), nullptr); }

TEST_F(NetsAndVariablesTest, Section5_HasNoPorts) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getPorts() == nullptr || mod->getPorts()->empty());
}

TEST_F(NetsAndVariablesTest, Section5_HasNoNets) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getNets() == nullptr || mod->getNets()->empty());
}

TEST_F(NetsAndVariablesTest, Section5_HasTwentyThreeVariables) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 22u);
}

TEST_F(NetsAndVariablesTest, Section5_HasOneNamedEvent) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNamedEvents(), nullptr);
  EXPECT_EQ(mod->getNamedEvents()->size(), 1u);
}

TEST_F(NetsAndVariablesTest, Section5_SimpleVariableTypesAreCorrectlyClassified) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  const VarExpectation kVars[] = {
      {"v_logic", hldb::AnyType::LogicTypespec, 0, -1, -1, -1},
      {"v_reg", hldb::AnyType::LogicTypespec, 0, -1, -1, -1},
      {"v_bit", hldb::AnyType::BitTypespec, 0, -1, -1, -1},
      {"v_byte", hldb::AnyType::ByteTypespec, 1, -1, -1, -1},
      {"v_shortint", hldb::AnyType::ShortIntTypespec, 1, -1, -1, -1},
      {"v_int", hldb::AnyType::IntTypespec, 1, -1, -1, -1},
      {"v_longint", hldb::AnyType::LongIntTypespec, 1, -1, -1, -1},
      {"v_integer", hldb::AnyType::IntegerTypespec, 1, -1, -1, -1},
      {"v_time", hldb::AnyType::TimeTypespec, -1, -1, -1, -1},
      {"v_real", hldb::AnyType::RealTypespec, -1, -1, -1, -1},
      {"v_realtime", hldb::AnyType::RealTypespec, -1, -1, -1, -1},
      {"v_shortreal", hldb::AnyType::ShortRealTypespec, -1, -1, -1, -1},
      {"v_string", hldb::AnyType::StringTypespec, -1, -1, -1, -1},
      {"v_chandle", hldb::AnyType::ChandleTypespec, -1, -1, -1, -1},
      {"v_vector", hldb::AnyType::LogicTypespec, 0, 3, 0, -1},
      {"v_reg_vector", hldb::AnyType::LogicTypespec, 0, 7, 0, -1},
      {"v_var_logic", hldb::AnyType::LogicTypespec, 0, -1, -1, -1},
      {"v_var_implicit", hldb::AnyType::LogicTypespec, 0, -1, -1, -1},
  };
  for (const VarExpectation &e : kVars) {
    CheckVar(mod, e);
  }
}

TEST_F(NetsAndVariablesTest, Section5_EnumVariableHasLogicBaseAndTwoConsts) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const venum = hldb::findByName<hldb::Variable>("v_enum", mod->getVariables());
  ASSERT_NE(venum, nullptr);
  ASSERT_NE(venum->getTypespec(), nullptr);

  // v_enum is an anonymous inline enum (no typedef), so its RefTypespec
  // actual is the EnumTypespec directly.
  const hldb::EnumTypespec *const et = venum->getTypespec()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "'v_enum' typespec does not resolve to an EnumTypespec";

  ASSERT_NE(et->getBaseTypespec(), nullptr);
  const hldb::Typespec *const base = et->getBaseTypespec()->getActual();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(static_cast<uint32_t>(base->getAnyType()), static_cast<uint32_t>(hldb::AnyType::LogicTypespec));
  // "enum logic [1:0] {...}" -- checked via the raw left/right bound
  // literals, not Range::getSize() (not computed in this flat model).
  const hldb::RangeCollection *const baseRanges = hldb::getRanges(base);
  ASSERT_NE(baseRanges, nullptr);
  ASSERT_EQ(baseRanges->size(), 1u);
  ASSERT_NE(baseRanges->at(0)->getLeftExpr(), nullptr);
  ASSERT_NE(baseRanges->at(0)->getRightExpr(), nullptr);
  EXPECT_EQ(ConstantIntValue(baseRanges->at(0)->getLeftExpr()), 1);
  EXPECT_EQ(ConstantIntValue(baseRanges->at(0)->getRightExpr()), 0);

  ASSERT_NE(et->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnumConsts()->size(), 2u);
  EXPECT_NE(hldb::findByName<hldb::EnumConst>("V_IDLE", et->getEnumConsts()), nullptr) << "enum const 'V_IDLE' not found";
  EXPECT_NE(hldb::findByName<hldb::EnumConst>("V_BUSY", et->getEnumConsts()), nullptr) << "enum const 'V_BUSY' not found";
}

TEST_F(NetsAndVariablesTest, Section5_TypedefPairTResolvesToPackedStruct) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *const tdt =
      hldb::findByName<hldb::TypedefTypespec>("pair_t", mod->getTypespecs());
  ASSERT_NE(tdt, nullptr) << "typedef 'pair_t' not found among module typespecs";

  ASSERT_NE(tdt->getTypedefAlias(), nullptr) << "typedef 'pair_t' has no alias";
  const hldb::StructTypespec *const st = tdt->getTypedefAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "'pair_t' does not resolve to a StructTypespec";
  EXPECT_TRUE(st->getPacked());

  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 2u);
  const hldb::TypespecMember *const hi = hldb::findByName<hldb::TypespecMember>("hi", st->getMembers());
  const hldb::TypespecMember *const lo = hldb::findByName<hldb::TypespecMember>("lo", st->getMembers());
  ASSERT_NE(hi, nullptr) << "struct member 'hi' not found";
  ASSERT_NE(lo, nullptr) << "struct member 'lo' not found";

  for (const hldb::TypespecMember *const member : {hi, lo}) {
    ASSERT_NE(member->getTypespec(), nullptr);
    const hldb::Typespec *const actual = member->getTypespec()->getActual();
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(actual->getAnyType()), static_cast<uint32_t>(hldb::AnyType::LogicTypespec));
    // "logic [3:0] hi/lo" -- checked via the raw left/right bound literals,
    // not Range::getSize() (not computed in this flat model).
    const hldb::RangeCollection *const ranges = hldb::getRanges(actual);
    ASSERT_NE(ranges, nullptr);
    ASSERT_EQ(ranges->size(), 1u);
    ASSERT_NE(ranges->at(0)->getLeftExpr(), nullptr);
    ASSERT_NE(ranges->at(0)->getRightExpr(), nullptr);
    EXPECT_EQ(ConstantIntValue(ranges->at(0)->getLeftExpr()), 3);
    EXPECT_EQ(ConstantIntValue(ranges->at(0)->getRightExpr()), 0);
  }
}

TEST_F(NetsAndVariablesTest, Section5_StructVariableResolvesThroughTypedefToPackedStruct) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const vstruct = hldb::findByName<hldb::Variable>("v_struct", mod->getVariables());
  ASSERT_NE(vstruct, nullptr);
  ASSERT_NE(vstruct->getTypespec(), nullptr);
  ASSERT_NE(vstruct->getTypespec()->getActual(), nullptr);

  const hldb::StructTypespec *const st = ResolveStruct(vstruct->getTypespec()->getActual());
  ASSERT_NE(st, nullptr) << "'v_struct' typespec does not resolve to a StructTypespec";
  EXPECT_TRUE(st->getPacked());
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u);
}

TEST_F(NetsAndVariablesTest, Section5_PackedTwoDimVariableHasTwoRanges) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const vp2d = hldb::findByName<hldb::Variable>("v_packed_2d", mod->getVariables());
  ASSERT_NE(vp2d, nullptr);
  ASSERT_NE(vp2d->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = vp2d->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'v_packed_2d' does not resolve to a LogicTypespec";

  // "logic [3:0][7:0] v_packed_2d;" -- checked via the raw left/right bound
  // literals of each range, not Range::getSize() (not computed in this flat
  // model). The two ranges should be {3:0} and {7:0}, in either order.
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "logic [3:0][7:0] should carry 2 packed ranges";
  const hldb::Range *const r0 = lt->getRanges()->at(0);
  const hldb::Range *const r1 = lt->getRanges()->at(1);
  ASSERT_NE(r0->getLeftExpr(), nullptr);
  ASSERT_NE(r0->getRightExpr(), nullptr);
  ASSERT_NE(r1->getLeftExpr(), nullptr);
  ASSERT_NE(r1->getRightExpr(), nullptr);
  const int32_t left0 = ConstantIntValue(r0->getLeftExpr());
  const int32_t right0 = ConstantIntValue(r0->getRightExpr());
  const int32_t left1 = ConstantIntValue(r1->getLeftExpr());
  const int32_t right1 = ConstantIntValue(r1->getRightExpr());
  EXPECT_TRUE((left0 == 3 && right0 == 0 && left1 == 7 && right1 == 0) ||
              (left0 == 7 && right0 == 0 && left1 == 3 && right1 == 0))
      << "expected ranges [3:0] and [7:0], got [" << left0 << ":" << right0 << "] and [" << left1 << ":" << right1
      << "]";
}

TEST_F(NetsAndVariablesTest, Section5_UnpackedArrayVariableIsStaticArrayOfInt) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const vua = hldb::findByName<hldb::Variable>("v_unpacked_array", mod->getVariables());
  ASSERT_NE(vua, nullptr);
  ASSERT_NE(vua->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = vua->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'v_unpacked_array' does not resolve to an ArrayTypespec";

  EXPECT_EQ(at->getArrayType(), vpiStaticArray);
  EXPECT_FALSE(at->getPacked());
  // "int v_unpacked_array [0:3];" -- checked via the raw left/right bound
  // literals, not Range::getSize() (not computed in this flat model). Note
  // the unpacked dimension is written ascending ([0:3]), unlike the
  // descending packed dimensions elsewhere in this file ([msb:0]).
  ASSERT_NE(at->getRange(), nullptr);
  ASSERT_NE(at->getRange()->getLeftExpr(), nullptr);
  ASSERT_NE(at->getRange()->getRightExpr(), nullptr);
  EXPECT_EQ(ConstantIntValue(at->getRange()->getLeftExpr()), 0);
  EXPECT_EQ(ConstantIntValue(at->getRange()->getRightExpr()), 3);

  const hldb::IntTypespec *const elem = hldb::getElemTypespec<hldb::IntTypespec>(at);
  ASSERT_NE(elem, nullptr) << "'v_unpacked_array' element typespec is not IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(NetsAndVariablesTest, Section5_HasOneInitialProcess) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  EXPECT_NE(initial, nullptr) << "expected an Initial process";
}

TEST_F(NetsAndVariablesTest, Section5_HasNoContAssigns) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  EXPECT_TRUE(mod->getContAssigns() == nullptr || mod->getContAssigns()->empty());
}

TEST_F(NetsAndVariablesTest, Section5_PrintExpectedVariables) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  ReportVarPresence(mod, "all_var_types",
                     {"v_logic", "v_reg", "v_bit", "v_byte", "v_shortint", "v_int", "v_longint", "v_integer",
                      "v_time", "v_real", "v_realtime", "v_shortreal", "v_string", "v_chandle", "v_enum",
                      "v_struct", "v_vector", "v_reg_vector", "v_packed_2d", "v_unpacked_array", "v_var_logic",
                      "v_var_implicit"});
}

TEST_F(NetsAndVariablesTest, Section5_TypespecActualMatchesExplicitType) {
  const hldb::Module *const mod = getModule("all_var_types");
  ASSERT_NE(mod, nullptr);
  const TypespecActualExpectation kVars[] = {
      {"v_logic", true},           // "logic v_logic;"
      {"v_reg", true},             // "reg v_reg;" -- reg is the type
      {"v_bit", true},             // "bit v_bit;"
      {"v_byte", true},            // "byte v_byte;"
      {"v_shortint", true},        // "shortint v_shortint;"
      {"v_int", true},             // "int v_int;"
      {"v_longint", true},         // "longint v_longint;"
      {"v_integer", true},         // "integer v_integer;"
      {"v_time", true},            // "time v_time;"
      {"v_real", true},            // "real v_real;"
      {"v_realtime", true},        // "realtime v_realtime;"
      {"v_shortreal", true},       // "shortreal v_shortreal;"
      {"v_string", true},          // "string v_string;"
      {"v_chandle", true},         // "chandle v_chandle;"
      {"v_event", true},           // "event v_event;"
      {"v_enum", true},            // "enum logic [1:0] {...} v_enum;"
      {"v_struct", true},          // "pair_t v_struct;" -- typedef name is the explicit type
      {"v_vector", true},          // "logic [3:0] v_vector;"
      {"v_reg_vector", true},      // "reg [7:0] v_reg_vector;" -- reg is the type
      {"v_packed_2d", true},       // "logic [3:0][7:0] v_packed_2d;"
      {"v_unpacked_array", true},  // "int v_unpacked_array [0:3];"
      {"v_var_logic", true},       // "var logic v_var_logic;" -- explicit "logic"
      // "var v_var_implicit;" -- "var" alone, no data type keyword; still resolves a
      // non-null actual (defaults to logic), same as every other omitted-type entry
      // in this file -- see the comment on Section1_TypespecActualMatchesExplicitType.
      {"v_var_implicit", true},
  };
  std::cout << "[TYPESPEC-ACTUAL] module 'all_var_types'\n";
  for (const TypespecActualExpectation &e : kVars) CheckVarTypespecActual(mod, "all_var_types", e);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
