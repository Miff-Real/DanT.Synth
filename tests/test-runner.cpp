#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_COLOUR_ANSI

#include "catch2/catch.hpp"

namespace {
class DanTReporter : public Catch::StreamingReporterBase<DanTReporter> {
 public:
  DanTReporter(Catch::ReporterConfig const& config) : Catch::StreamingReporterBase<DanTReporter>(config) {}

  static std::set<Catch::Verbosity> getSupportedVerbosities() { return {Catch::Verbosity::Normal}; }

  static std::string getDescription() { return "DanT custom test reporter"; }

  void testCaseStarting(Catch::TestCaseInfo const& testCaseInfo) override {
    StreamingReporterBase::testCaseStarting(testCaseInfo);
    this->stream << Catch::Colour(Catch::Colour::Cyan) << "TEST [ " << testCaseInfo.name << " ]"
                 << Catch::Colour(Catch::Colour::None) << "\n";
  }

  void sectionStarting(Catch::SectionInfo const& sectionInfo) override {
    StreamingReporterBase::sectionStarting(sectionInfo);
    if (this->currentTestCaseInfo && sectionInfo.name != this->currentTestCaseInfo->name) {
      this->stream << Catch::Colour(Catch::Colour::Blue) << "  SECTION [ " << sectionInfo.name << " ]"
                   << Catch::Colour(Catch::Colour::None) << "\n";
    }
  }

  void assertionStarting(Catch::AssertionInfo const& assertionInfo) override {
    // pure virtual
  }

  bool assertionEnded(Catch::AssertionStats const& assertionStats) override {
    const Catch::AssertionResult& assertionResult = assertionStats.assertionResult;
    this->stream << "    * ";
    if (!assertionStats.infoMessages.empty()) {
      for (const auto& messageInfo : assertionStats.infoMessages) {
        this->stream << Catch::Colour(Catch::Colour::Yellow) << messageInfo.message
                     << Catch::Colour(Catch::Colour::None) << " ";
      }
    }
    if (assertionResult.hasMessage()) {
      this->stream << Catch::Colour(Catch::Colour::Yellow) << assertionResult.getMessage()
                   << Catch::Colour(Catch::Colour::None) << " ";
    }
    this->stream << "check(" << Catch::Colour(Catch::Colour::Yellow) << assertionResult.getExpandedExpression()
                 << Catch::Colour(Catch::Colour::None) << ") = ";
    if (assertionResult.succeeded()) {
      this->stream << Catch::Colour(Catch::Colour::ResultSuccess) << "PASSED" << Catch::Colour(Catch::Colour::None);
    } else {
      this->stream << Catch::Colour(Catch::Colour::ResultError) << "FAILED" << Catch::Colour(Catch::Colour::None);
    }
    this->stream << "\n";
    return true;
  }

  void sectionEnded(Catch::SectionStats const& sectionStats) override {
    StreamingReporterBase::sectionEnded(sectionStats);
  }

  void testCaseEnded(Catch::TestCaseStats const& testCaseStats) override {
    StreamingReporterBase::testCaseEnded(testCaseStats);
    this->stream << "\n\n";
  }

  void testRunEnded(Catch::TestRunStats const& testRunStats) override {
    StreamingReporterBase::testRunEnded(testRunStats);
    if (testRunStats.totals.assertions.allPassed()) {
      this->stream << "===============================================================================\n";
      this->stream << Catch::Colour(Catch::Colour::ResultSuccess) << "All tests passed ("
                   << testRunStats.totals.assertions.passed << " assertions in "
                   << testRunStats.totals.testCases.total() << " test cases)" << Catch::Colour(Catch::Colour::None)
                   << "\n";
    } else {
      this->stream << "===============================================================================\n";
      this->stream << Catch::Colour(Catch::Colour::ResultError)
                   << "Tests failed! Total: " << testRunStats.totals.testCases.total()
                   << ", Passed: " << testRunStats.totals.testCases.passed
                   << ", Failed: " << testRunStats.totals.testCases.failed << "\n";
      this->stream << "Assertions: Passed: " << testRunStats.totals.assertions.passed
                   << ", Failed: " << testRunStats.totals.assertions.failed << Catch::Colour(Catch::Colour::None)
                   << "\n";
    }
  }
};

CATCH_REGISTER_REPORTER("dant", DanTReporter)

}  // namespace
