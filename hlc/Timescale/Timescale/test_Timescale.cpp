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

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/instance.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/program.h>
#include <hldb/source_file.h>

#include <string_view>
#include <vector>

// Time unit and time precision of design elements, IEEE 1800-2023:
//   Sec 3.14     legal values: 1, 10 or 100 of s, ms, us, ns, ps, fs
//   Sec 3.14.2.1 `timescale
//   Sec 3.14.2.2 timeunit / timeprecision declarations
//   Sec 3.14.2.3 precedence when an element does not declare its own:
//                (a) nested -> enclosing module/interface
//                (b) else the last `timescale in the compilation unit
//                (c) else the compilation-unit timeunit/timeprecision
//                (d) else the default
//   Sec 22.7     `timescale, and `resetall resets it to the default
//
// Every file is its own compilation unit (-fileunit). Values are stored as the power
// of ten of one second (1ns = -9, 100ps = -10); 0 means "not set", i.e. the default.
//
//   dut.sv                     original nested/`timescale cases (m1, m11, m12, m2)
//   timescale.sv               rule (b), replacement, `resetall
//   decl.sv                    own declarations in every form; rule (d)
//   nested.sv                  rule (a)
//   cu_timeunit.sv             rule (c)
//   units.sv                   every legal magnitude and unit
//   errors.sv                  illegal declarations and `timescale values
//   errors_timescale_inside.sv `timescale inside a design element
//   errors_cu_late.sv          compilation-unit timeunit after another item

namespace hlc {

namespace {
struct Expected {
  std::string_view m_name;
  int32_t m_unit;
  int32_t m_precision;
};

// Searches an element and every module/interface nested inside it.
const hldb::Instance *findInstanceIn(const hldb::Instance *instance, std::string_view name);

template <typename T>
const hldb::Instance *findInstanceInCollection(const std::vector<T *> *collection, std::string_view name) {
  if (collection == nullptr) return nullptr;
  for (const T *item : *collection) {
    if (const hldb::Instance *const found = findInstanceIn(item, name)) return found;
  }
  return nullptr;
}

const hldb::Instance *findInstanceIn(const hldb::Instance *instance, std::string_view name) {
  if (instance == nullptr) return nullptr;
  if (instance->getName() == name) return instance;
  if (const hldb::Module *const module = any_cast<hldb::Module>(instance)) {
    if (const hldb::Instance *const found = findInstanceInCollection(module->getModules(), name)) return found;
    if (const hldb::Instance *const found = findInstanceInCollection(module->getInterfaces(), name)) return found;
  } else if (const hldb::Interface *const intf = any_cast<hldb::Interface>(instance)) {
    if (const hldb::Instance *const found = findInstanceInCollection(intf->getInterfaces(), name)) return found;
  }
  return nullptr;
}
}  // namespace

class TimescaleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Timescale.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getM1() { return hldb::findByName<hldb::Module>("m1", m_design->getAllModules()); }

  // Every top-level module, interface, program and package, and everything nested in them.
  static const hldb::Instance *findInstance(std::string_view name) {
    if (const hldb::Instance *const found = findInstanceInCollection(m_design->getAllModules(), name)) return found;
    if (const hldb::Instance *const found = findInstanceInCollection(m_design->getAllInterfaces(), name)) return found;
    if (const hldb::Instance *const found = findInstanceInCollection(m_design->getAllPrograms(), name)) return found;
    return findInstanceInCollection(m_design->getAllPackages(), name);
  }

  static const hldb::SourceFile *findSourceFile(std::string_view name) {
    return hldb::findByName<hldb::SourceFile>(name, m_design->getSourceFiles());
  }

  // True if an error (any code with ERROR, SYNTAX or FATAL severity) is reported on
  // 'line' of the file named 'fileName'. The standard says which constructs are errors,
  // not which diagnostic reports them.
  static bool hasErrorAt(std::string_view fileName, uint32_t line) {
    const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      const ErrorDefinition::ErrorMap::const_iterator it = infos.find(error.getType());
      if (it == infos.cend()) continue;
      const ErrorDefinition::ErrorSeverity severity = it->second.m_severity;
      if ((severity != ErrorDefinition::ERROR) && (severity != ErrorDefinition::SYNTAX) &&
          (severity != ErrorDefinition::FATAL)) {
        continue;
      }
      for (const Location &loc : error.getLocations()) {
        if (loc.m_line != line) continue;
        const std::string_view path = FileSystem::toPath(loc.m_fileId);
        if ((path.size() > fileName.size()) && (path.substr(path.size() - fileName.size()) == fileName) &&
            (path[path.size() - fileName.size() - 1] == '/')) {
          return true;
        }
      }
    }
    return false;
  }

  static void expectAll(const std::vector<Expected> &expected) {
    for (const Expected &e : expected) {
      const hldb::Instance *const instance = findInstance(e.m_name);
      ASSERT_NE(instance, nullptr) << "design element '" << e.m_name << "' not found";
      EXPECT_EQ(instance->getTimeUnit(), e.m_unit) << e.m_name << ": time unit";
      EXPECT_EQ(instance->getTimePrecision(), e.m_precision) << e.m_name << ": time precision";
    }
  }
};

// ----
// dut.sv
// ----

// The SourceFile is the compilation-unit scope. dut.sv has no timeunit/timeprecision
// outside its modules, and per IEEE 1800-2023 Sec 3.14.2.3 "The time unit of the
// compilation-unit scope can only be set by a timeunit declaration, not a `timescale
// directive", so the `timescale 1 ns/1 ps before m2 leaves it unset.
TEST_F(TimescaleTest, SourceFileTimescale) {
  const hldb::SourceFile *const s = findSourceFile("dut.sv");
  ASSERT_NE(s, nullptr) << "SourceFile s is null";
  EXPECT_EQ(s->getTimeUnit(), 0);
  EXPECT_EQ(s->getTimePrecision(), 0);
}

// m1: explicit "timeunit 10ns/1ps;" -> unit 10ns (1e-8), precision 1ps (1e-12).
TEST_F(TimescaleTest, M1ExplicitTimescale) {
  const hldb::Module *const m1 = getM1();
  ASSERT_NE(m1, nullptr) << "Module m1 is null";
  EXPECT_EQ(m1->getTimeUnit(), -8);
  EXPECT_EQ(m1->getTimePrecision(), -12);
}

// m11 (nested in m1): explicit "timeunit 10ns/10ps;" -> unit 10ns (1e-8),
// precision 10ps (1e-11).
TEST_F(TimescaleTest, M11ExplicitTimescale) {
  const hldb::Module *const m1 = getM1();
  ASSERT_NE(m1, nullptr) << "Module m1 is null";
  const hldb::Module *const m11 = hldb::findByName<hldb::Module>("m11", m1->getModules());
  ASSERT_NE(m11, nullptr) << "Module m11 is null";
  EXPECT_EQ(m11->getTimeUnit(), -8);
  EXPECT_EQ(m11->getTimePrecision(), -11);
}

// m12 (nested in m1): only "timeprecision 1ps;" -- no timeunit of its own.
// Per IEEE 1800-2023 3.14.2.3 rule (a), since m12 is nested inside m1, its
// time unit shall be inherited from the enclosing module m1 (10 ns, -8), not
// left at an unspecified/default value.
TEST_F(TimescaleTest, M12InheritsTimeUnitFromEnclosingModule) {
  const hldb::Module *const m1 = getM1();
  ASSERT_NE(m1, nullptr) << "Module m1 is null";
  const hldb::Module *const m12 = hldb::findByName<hldb::Module>("m12", m1->getModules());
  ASSERT_NE(m12, nullptr) << "Module m12 is null";
  EXPECT_EQ(m12->getTimeUnit(), -8) << "m12 must inherit m1's time unit per Sec 3.14.2.3 rule (a)";
  EXPECT_EQ(m12->getTimePrecision(), -12);
}

// m2 (not nested, no timeunit of its own): per Sec 3.14.2.3 rule (b),
// inherits from the last `timescale 1 ns/1 ps directive -> -9/-12.
TEST_F(TimescaleTest, M2InheritsFromTimescaleDirective) {
  const hldb::Module *const m2 = hldb::findByName<hldb::Module>("m2", m_design->getAllModules());
  ASSERT_NE(m2, nullptr) << "Module m2 is null";
  EXPECT_EQ(m2->getTimeUnit(), -9);
  EXPECT_EQ(m2->getTimePrecision(), -12);
}

// ----
// decl.sv -- Sec 3.14.2.2: an element's own timeunit / timeprecision, in every form
// the grammar allows (timeunits_declaration, Annex A.1.2). Nothing else in the file,
// so anything not declared stays at the default (rule d).
// ----
TEST_F(TimescaleTest, OwnDeclarations) {
  expectAll({
      {"d_unit_only", -10, 0},         // timeunit 100ps;
      {"d_unit_slash", -10, -14},      // timeunit 100ps / 10fs;              (example E)
      {"d_unit_then_prec", -10, -14},  // timeunit 100ps; timeprecision 10fs; (example D)
      {"d_prec_then_unit", -10, -14},  // timeprecision 10fs; timeunit 100ps;
      {"d_prec_only", 0, -12},         // timeprecision 1ps;
      {"d_unit_equals_prec", -9, -9},  // timeunit 1ns / 1ns; precision may equal the unit
      {"d_repeat_match", -9, -12},     // repeated later, matching
  });
}

TEST_F(TimescaleTest, OwnDeclarationsOnEveryElementKind) {
  // Sec 3.14.2.2: "at most one time unit and one time precision for any module,
  // program, package, or interface definition".
  expectAll({
      {"d_if", -6, -9},    // interface: timeunit 1us / 1ns;
      {"d_prog", -5, -7},  // program:   timeunit 10us / 100ns;
      {"d_pkg", -4, -6},   // package:   timeunit 100us / 1us;
  });
}

TEST_F(TimescaleTest, RepeatedMatchingDeclarationIsLegal) {
  // Sec 3.14.2.2: the declarations "can be repeated as later items, but shall match
  // the previous declaration within the current time scope."
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "d_repeat_match"), nullptr);
}

TEST_F(TimescaleTest, TimescaleDoesNotCrossCompilationUnits) {
  // Sec 3.14.2.1: "The `timescale directive only affects the current compilation
  // unit; it does not span multiple compilation units." timescale.sv ends with
  // `timescale 10 ns / 1 ns and decl.sv is compiled right after it as its own
  // compilation unit, so d_none, with nothing of its own, gets the default (rule d).
  expectAll({{"d_none", 0, 0}});
}

// ----
// nested.sv -- rule (a): a nested module or interface without its own declaration
// inherits from the enclosing module or interface.
// ----
TEST_F(TimescaleTest, NestedInheritsFromEnclosingElement) {
  expectAll({
      {"n_outer", -8, -12},       // timeunit 10ns / 1ps;
      {"n_inner_none", -8, -12},  // both inherited
      {"n_inner_prec", -8, -13},  // own precision 100fs, unit inherited
      {"n_inner_unit", -6, -12},  // own unit 1us, precision inherited
      {"n_mid", -8, -12},         // inherited ...
      {"n_deep", -8, -12},        // ... and again, two levels down
      {"n_mid_own", -7, -8},      // timeunit 100ns / 10ns;
      {"n_deep_own", -7, -8},     // the nearest enclosing element wins
  });
}

TEST_F(TimescaleTest, NestedInterfaceInheritsFromEnclosingElement) {
  // Rule (a) covers "the module or interface definition", nested in a module or
  // in an interface.
  expectAll({
      {"n_inner_if", -8, -12},  // interface nested in module n_outer
      {"n_outer_if", -7, -8},   // timeunit 100ns / 10ns;
      {"n_nested_if", -7, -8},  // interface nested in interface n_outer_if
  });
}

TEST_F(TimescaleTest, NestedInsideDefaultElementGetsTheDefault) {
  // Rule (a) inherits whatever the enclosing element has, here the default.
  expectAll({
      {"n_outer_none", 0, 0},
      {"n_inner_of_none", 0, 0},
  });
}

// ----
// timescale.sv -- rule (b): the last `timescale in the compilation unit.
// ----
TEST_F(TimescaleTest, TimescaleAppliesToFollowingElements) {
  // Sec 3.14.2.1 example: `timescale 1ns / 10ps "affects both module A and module B".
  expectAll({
      {"t_a", -9, -11},
      {"t_b", -9, -11},
  });
}

TEST_F(TimescaleTest, TimescaleAppliesToEveryElementKind) {
  // Rule (b) applies to "a module, program, package, or interface definition".
  expectAll({
      {"t_if", -9, -11},
      {"t_prog", -9, -11},
      {"t_pkg", -9, -11},
  });
}

TEST_F(TimescaleTest, OwnDeclarationBeatsTimescale) {
  // Sec 3.14.2.1: `timescale applies to design elements "that do not have timeunit
  // and timeprecision constructs specified within the design element". Unit and
  // precision each follow the precedence on their own (Sec 3.14.2.3: "the same
  // precedence as with time units"), so a missing one still comes from the `timescale.
  expectAll({
      {"t_own", -10, -15},       // timeunit 100ps / 1fs;
      {"t_prec_only", -9, -12},  // timeprecision 1ps; unit from `timescale
      {"t_unit_only", -8, -11},  // timeunit 10ns; precision from `timescale
  });
}

TEST_F(TimescaleTest, NestingBeatsTimescale) {
  // Rule (a) is checked before rule (b).
  expectAll({
      {"t_outer", -6, -9},  // timeunit 1us / 1ns;
      {"t_inner", -6, -9},  // nested: from t_outer, not from `timescale 1ns / 10ps
      {"t_outer_none", -9, -11},
      {"t_inner_of_none", -9, -11},
  });
}

TEST_F(TimescaleTest, LaterTimescaleReplacesEarlierOne) {
  // Sec 3.14.2.1 example: "A second `timescale directive replaces the first directive,
  // specifying a time unit of 1 ps and precision of 1 ps ... for module C." Also the
  // Sec 22.7 examples `timescale 10 us / 100 ns and `timescale 10 ns / 1 ns.
  expectAll({
      {"t_c", -12, -12},
      {"t_spaced", -5, -7},
      {"t_test", -8, -9},
  });
}

TEST_F(TimescaleTest, ResetallRestoresTheDefault) {
  // Sec 22.7: "If there is no `timescale specified or it has been reset by a
  // `resetall directive, the default time unit and precision are tool-specific."
  expectAll({{"t_after_resetall", 0, 0}});
}

TEST_F(TimescaleTest, TimescaleDoesNotSetTheCompilationUnit) {
  // Sec 3.14.2.3: "The time unit of the compilation-unit scope can only be set by a
  // timeunit declaration, not a `timescale directive."
  const hldb::SourceFile *const s = findSourceFile("timescale.sv");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getTimeUnit(), 0);
  EXPECT_EQ(s->getTimePrecision(), 0);
}

// ----
// cu_timeunit.sv -- rule (c): the compilation-unit timeunit / timeprecision.
// ----
TEST_F(TimescaleTest, CompilationUnitDeclarationSetsTheSourceFile) {
  // timeunit 1us; timeprecision 1ns; outside every design element.
  const hldb::SourceFile *const s = findSourceFile("cu_timeunit.sv");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getTimeUnit(), -6);
  EXPECT_EQ(s->getTimePrecision(), -9);
}

TEST_F(TimescaleTest, CompilationUnitAppliesToElementsWithoutTheirOwn) {
  expectAll({
      {"c_none", -6, -9},
      {"c_unit_only", -8, -9},   // timeunit 10ns; precision from the compilation unit
      {"c_prec_only", -6, -12},  // timeprecision 1ps; unit from the compilation unit
      {"c_if", -6, -9},
      {"c_prog", -6, -9},
      {"c_pkg", -6, -9},
  });
}

TEST_F(TimescaleTest, NestingBeatsCompilationUnit) {
  // Rule (a) is checked before rule (c).
  expectAll({
      {"c_outer", -7, -8},  // timeunit 100ns / 10ns;
      {"c_inner", -7, -8},
  });
}

TEST_F(TimescaleTest, TimescaleBeatsCompilationUnit) {
  // Rule (b) is checked before rule (c). `timescale 1ps / 1fs
  expectAll({{"c_after_timescale", -12, -15}});
}

TEST_F(TimescaleTest, ResetallDoesNotResetCompilationUnit) {
  // Sec 22.3: `resetall resets compiler directives; the compilation-unit timeunit is a
  // declaration, so after `resetall rule (c) applies again.
  //
  // This is an interpretation. Sec 22.7 says that once a `timescale "has been reset by a
  // `resetall directive" the default applies, which reads as "no `timescale in effect",
  // so rule (b) no longer applies and rule (c) does. Rule (b)'s own wording, "if a
  // `timescale directive has been previously specified", could be read as still
  // holding after the reset.
  expectAll({{"c_after_resetall", -6, -9}});
}

// ----
// units.sv -- every legal value (Sec 3.14 Table 3-1 and Sec 22.7: magnitude 1, 10
// or 100 of s, ms, us, ns, ps or fs), as a time unit and as a time precision.
// ----
TEST_F(TimescaleTest, EveryLegalTimeUnit) {
  // timeunit <value> / 1fs;
  expectAll({
      {"u_unit_1s", 0, -15},    {"u_unit_10s", 1, -15},    {"u_unit_100s", 2, -15},
      {"u_unit_1ms", -3, -15},  {"u_unit_10ms", -2, -15},  {"u_unit_100ms", -1, -15},
      {"u_unit_1us", -6, -15},  {"u_unit_10us", -5, -15},  {"u_unit_100us", -4, -15},
      {"u_unit_1ns", -9, -15},  {"u_unit_10ns", -8, -15},  {"u_unit_100ns", -7, -15},
      {"u_unit_1ps", -12, -15}, {"u_unit_10ps", -11, -15}, {"u_unit_100ps", -10, -15},
      {"u_unit_1fs", -15, -15}, {"u_unit_10fs", -14, -15}, {"u_unit_100fs", -13, -15},
  });
}

TEST_F(TimescaleTest, EveryLegalTimePrecision) {
  // timeunit 100s / <value>;
  expectAll({
      {"u_prec_1s", 2, 0},    {"u_prec_10s", 2, 1},    {"u_prec_100s", 2, 2},
      {"u_prec_1ms", 2, -3},  {"u_prec_10ms", 2, -2},  {"u_prec_100ms", 2, -1},
      {"u_prec_1us", 2, -6},  {"u_prec_10us", 2, -5},  {"u_prec_100us", 2, -4},
      {"u_prec_1ns", 2, -9},  {"u_prec_10ns", 2, -8},  {"u_prec_100ns", 2, -7},
      {"u_prec_1ps", 2, -12}, {"u_prec_10ps", 2, -11}, {"u_prec_100ps", 2, -10},
      {"u_prec_1fs", 2, -15}, {"u_prec_10fs", 2, -14}, {"u_prec_100fs", 2, -13},
  });
}

// The u_unit_1s and u_prec_1s rows above expect 0, which is also the "not set" value,
// so on their own they would pass even if the 1s declaration were ignored. This test
// is the one that tells the two apart: both elements declare a time unit and a
// precision, so neither may be reported as missing time information.
TEST_F(TimescaleTest, OneSecondIsDistinguishableFromUnset) {
  GTEST_SKIP() << "Known HLC gap: 1s encodes as 10^0 = 0, the same value used for \"not set\", so "
                  "u_unit_1s and u_prec_1s are reported as having no time unit / precision although "
                  "they declare one (Sec 3.14 Table 3-1 lists s as a legal unit).";
  EXPECT_EQ(findError(ErrorDefinition::PA_MISSING_TIMEUNIT, "u_unit_1s"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "u_unit_1s"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_MISSING_TIMEUNIT, "u_prec_1s"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "u_prec_1s"), nullptr);
}

// ----
// errors.sv / errors_timescale_inside.sv -- each element breaks one rule.
// ----
TEST_F(TimescaleTest, PrecisionLongerThanUnitInDeclarationIsAnError) {
  // Sec 3.14: "The time precision of a design element shall be at least as precise as
  // the time unit; it cannot be a longer unit of time than the time unit."
  // timeunit 1ns / 1us;
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "e_coarse_decl"), nullptr);
}

TEST_F(TimescaleTest, MismatchedRepeatIsAnError) {
  // Sec 3.14.2.2: a repeat "shall match the previous declaration".
  // timeunit 1ns; logic x; timeunit 10ns;
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "e_repeat_mismatch"), nullptr);
}

TEST_F(TimescaleTest, DeclarationAfterAnotherItemIsAnError) {
  // Sec 3.14.2.2: "the timeunit and timeprecision declarations shall precede any other
  // items in the current time scope." logic x; timeunit 1ns;
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "e_not_first"), nullptr);
}

TEST_F(TimescaleTest, StepAsTimeUnitIsAnError) {
  // timeunit 1step; (errors.sv:20). Annex A time_unit ::= s | ms | us | ns | ps | fs has
  // no step, so this is a syntax error; Sec 3.14.3 also says "a step cannot be used to
  // set or modify either the precision or the time unit." Any error is correct.
  EXPECT_TRUE(hasErrorAt("errors.sv", 20));
}

TEST_F(TimescaleTest, TimeunitMagnitudeMustBe1Or10Or100) {
  GTEST_SKIP() << "Known HLC gap: timeunit 3ns; and timeunit 1.5ns; are accepted (and stored as 1ns). "
                  "Sec 3.14: time values are \"s, ms, us, ns, ps, and fs with an order of magnitude of 1, "
                  "10, or 100\".";
  EXPECT_TRUE(hasErrorAt("errors.sv", 24)) << "timeunit 3ns;";
  EXPECT_TRUE(hasErrorAt("errors.sv", 28)) << "timeunit 1.5ns;";
}

TEST_F(TimescaleTest, WhitespaceInsideTimeLiteralIsAnError) {
  GTEST_SKIP() << "Known HLC gap: timeunit 10 ns; and timeprecision 1 ps; are accepted. Annex A footnote 49: "
                  "\"The unsigned number or fixed-point number in time_literal shall not be followed by "
                  "white_space.\" (`timescale 1 ns / 1 ps is fine: it is not a time_literal.)";
  EXPECT_TRUE(hasErrorAt("errors.sv", 32)) << "timeunit 10 ns;";
  EXPECT_TRUE(hasErrorAt("errors.sv", 36)) << "timeprecision 1 ps;";
}

TEST_F(TimescaleTest, PrecisionLongerThanInheritedUnitIsAnError) {
  GTEST_SKIP() << "Known HLC gap: a nested module with timeprecision 1us; inside a parent with a 1ns unit "
                  "is accepted. Sec 3.14: \"The time precision of a design element shall be at least as "
                  "precise as the time unit\", and its unit is the inherited 1ns (Sec 3.14.2.3 a).";
  EXPECT_TRUE(hasErrorAt("errors.sv", 42)) << "timeprecision 1us; in e_child_coarse";
}

TEST_F(TimescaleTest, CompilationUnitTimeunitAfterAnItemIsAnError) {
  GTEST_SKIP() << "Known HLC gap: a compilation-unit timeunit after a module is accepted. Sec 3.14.2.2: "
                  "\"There shall be at most one time unit and one time precision for any ... "
                  "compilation-unit scope ... the timeunit and timeprecision declarations shall precede any "
                  "other items in the current time scope.\"";
  EXPECT_TRUE(hasErrorAt("errors_cu_late.sv", 9)) << "timeunit 1ns; after module e_cu_before";
}

TEST_F(TimescaleTest, TimescaleInsideDesignElementIsAnError) {
  // Sec 22.7: "It shall be illegal for the `timescale directive to be specified within
  // a design element."
  EXPECT_NE(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT, "`timescale"), nullptr);
}

TEST_F(TimescaleTest, TimescalePrecisionLongerThanUnitIsAnError) {
  GTEST_SKIP() << "Known HLC gap: `timescale 1ns / 1us is accepted without a diagnostic. Sec 22.7: \"The "
                  "time_precision argument shall be at least as precise as the time_unit argument\" "
                  "(PA_TIMESCALE_INVALID_SCALE is defined but never reported).";
  EXPECT_NE(findError(ErrorDefinition::PA_TIMESCALE_INVALID_SCALE), nullptr);
}

TEST_F(TimescaleTest, TimescaleInvalidMagnitudeIsAnError) {
  GTEST_SKIP() << "Known HLC gap: `timescale 3ns / 1ps is accepted without a diagnostic. Sec 22.7: \"the "
                  "valid integers are 1, 10, and 100\" (PA_TIMESCALE_INVALID_VALUE is defined but never "
                  "reported).";
  EXPECT_NE(findError(ErrorDefinition::PA_TIMESCALE_INVALID_VALUE), nullptr);
}

TEST_F(TimescaleTest, ElementWithoutTimeInfoIsReported) {
  // Sec 3.14.2.3: "It shall be an error if some design elements have a time unit and
  // precision specified and others do not." d_none has neither, while most elements
  // in this design do, so d_none is reported.
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "d_none"), nullptr);
}

TEST_F(TimescaleTest, ElementWithoutTimeInfoIsAnError) {
  GTEST_SKIP() << "Known HLC gap: Sec 3.14.2.3 \"It shall be an error if some design elements have a time "
                  "unit and precision specified and others do not\", but PA_NOTIMESCALE_INFO is a WARNING.";
  ASSERT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "d_none"), nullptr);
  const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator it = infos.find(ErrorDefinition::PA_NOTIMESCALE_INFO);
  ASSERT_NE(it, infos.cend());
  EXPECT_EQ(it->second.m_severity, ErrorDefinition::ERROR);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
