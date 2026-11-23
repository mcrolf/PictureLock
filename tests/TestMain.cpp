#include <juce_gui_basics/juce_gui_basics.h>

int main()
{
    // The processor tests need a message manager for settings change messages and async state restore.
    juce::ScopedJuceInitialiser_GUI gui;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    return failures == 0 ? 0 : 1;
}
