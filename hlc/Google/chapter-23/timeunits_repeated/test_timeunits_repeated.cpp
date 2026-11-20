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

// Test for tests/Google/chapter-23/23.2.4--timeunits-repeated.sv
//
// THE SOURCE
//
//   21: module m;
//   22:   timeunit 1ns;
//   23:   timeprecision 1ps;
//   24:   logic x;
//   25:   timeunit 1ns;
//   26:   timeprecision 1ps;
//   27: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-5, footnote 3: "A timeunits_declaration shall be legal as a
// non_port_module_item, non_port_interface_item, non_port_program_item, or
// package_item only if it repeats and matches a previous timeunits_declaration
// within the same time scope." Lines 22-23 are the module's own
// timeunits_declaration (Syntax 23-1); lines 25-26 repeat it exactly, so they
// are legal, and the module's time unit and precision stay 1ns and 1ps. The
// object model reports them as vpiTimeUnit and vpiTimePrecision (37.10), the
// power-of-ten exponent relative to one second: -9 and -12.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   TimeUnitIsOneNanosecond    vpiTimeUnit -9
//   TimePrecisionIsOnePicosecond   vpiTimePrecision -12
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - A mismatching repeat. 23.2.4--timeunits-mismatch-illegal.sv covers it.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.4--timeunits-repeated.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}
}  // namespace

class TimeunitsRepeatedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "timeunits_repeated.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // The module named 'name' defined in the file under test, or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == name)) return module;
    }
    return nullptr;
  }
};

TEST_F(TimeunitsRepeatedTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(TimeunitsRepeatedTest, TimeUnitIsOneNanosecond) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(integerProperty(module, vpiTimeUnit), -9);
}

TEST_F(TimeunitsRepeatedTest, TimePrecisionIsOnePicosecond) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(integerProperty(module, vpiTimePrecision), -12);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
