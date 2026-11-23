// GLOBAL CONTROL - standalone application (no audio processing, independent from the audio session).
//   --app-port N / --bridge-port N   override the UDP ports (default 47120 / 47121, always 127.0.0.1)
//   --screenshot file.png [--delay ms] [--demo]   render the window to a PNG and quit (documentation / CI);
//                                                 --demo assigns a few parameters first
#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel.h"
#include "MainComponent.h"

namespace gc::ui {

static juce::String argValue (const juce::StringArray& args, const juce::String& key, const juce::String& def = {})
{
    const int i = args.indexOf (key);
    return i >= 0 && i + 1 < args.size() ? args[i + 1] : def;
}

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow (AppController& app)
        : juce::DocumentWindow ("GLOBAL CONTROL", col::bg, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (app), true);
        setResizable (true, true);
        setResizeLimits (1180, 780, 3840, 2400);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class GlobalControlApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "GLOBAL CONTROL"; }
    const juce::String getApplicationVersion() override { return JUCE_STRINGIFY (GC_VERSION); }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& cmd) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);
        const auto args = juce::StringArray::fromTokens (cmd, true);
        app = std::make_unique<AppController>();
        if (! app->start (argValue (args, "--app-port", "47120").getIntValue(), argValue (args, "--bridge-port", "47121").getIntValue()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "GLOBAL CONTROL", app->getStartError());
        window = std::make_unique<MainWindow> (*app);

        const auto shot = argValue (args, "--screenshot");
        if (shot.isNotEmpty()) {
            const bool demo = args.contains ("--demo");
            const int delay = argValue (args, "--delay", "3000").getIntValue();
            if (demo) runDemo();
            juce::Timer::callAfterDelay (delay, [this, shot] {
                if (auto* c = window->getContentComponent()) {
                    const auto img = c->createComponentSnapshot (c->getLocalBounds(), true, 2.0f);
                    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (shot.unquoted());
                    f.deleteFile();
                    juce::FileOutputStream os (f);
                    juce::PNGImageFormat().writeImageToStream (img, os);
                }
                quit();
            });
        }
    }

    void runDemo()
    {
        // documentation helper: waits for the scan, then assigns typical compressor parameters
        juce::Timer::callAfterDelay (1200, [this] {
            if (! app->getSnapshot().valid || app->selectedPluginParameters() == nullptr) { runDemo(); return; }
            app->assignParameters ({ "Threshold" }, 0);
            app->assignParameters ({ "Attack", "Release", "Ratio" }, 1);
            juce::Timer::callAfterDelay (600, [this] {
                app->setMacroMode (0, MacroMode::Relative);
                auto s = app->getMacro (0).settings; s.relativeSpan = 6; app->setMacroSettings (0, s);
                app->setMacroKnob (0, 0.5 - 1.0 / 6.0);
                app->setMacroMode (1, MacroMode::Relative);
            });
        });
    }

    void shutdown() override
    {
        window.reset();
        app.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override { if (window) window->toFront (true); }

private:
    GCLookAndFeel laf;
    std::unique_ptr<AppController> app;
    std::unique_ptr<MainWindow> window;
};

} // namespace gc::ui

START_JUCE_APPLICATION (gc::ui::GlobalControlApplication)
