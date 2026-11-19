#include "BridgeInstaller.h"

namespace gc {

juce::File BridgeInstaller::findSourceDir()
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::Array<juce::File> candidates {
        exe.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile ("Bridge"),   // GLOBAL CONTROL.app/Contents/Resources/Bridge
        exe.getParentDirectory().getChildFile ("Bridge"),
    };
   #ifdef GC_SOURCE_DIR
    candidates.add (juce::File (GC_SOURCE_DIR).getChildFile ("AbletonBridge").getChildFile ("Build"));
   #endif
    for (const auto& c : candidates)
        if (c.getChildFile (kScriptFile).existsAsFile()) return c;
    return {};
}

BridgeEnvironment BridgeInstaller::inspect()
{
    BridgeEnvironment e;
   #if JUCE_MAC
    for (const auto& app : juce::File ("/Applications").findChildFiles (juce::File::findDirectories, false, "Ableton Live*.app")) {
        e.liveApps.add (app);
        // Live Suite ships Max inside the application bundle  [VERIFICA: path per Live version]
        for (const auto& p : { "Contents/App-Resources/Max", "Contents/App-Resources/Max/Max.app" })
            if (app.getChildFile (p).exists()) { e.maxForLiveFound = true; e.maxForLiveDetail = "bundled with " + app.getFileNameWithoutExtension(); }
    }
    for (const auto& max : juce::File ("/Applications").findChildFiles (juce::File::findDirectories, false, "Max*.app"))
        if (! e.maxForLiveFound) { e.maxForLiveFound = true; e.maxForLiveDetail = max.getFileName() + " (standalone; Live Suite or the Max for Live add-on is required)"; }
   #endif
    if (! e.maxForLiveFound && e.maxForLiveDetail.isEmpty())
        e.maxForLiveDetail = e.liveApps.isEmpty() ? "Ableton Live not found in /Applications" : "Max for Live not found (Live Suite or the Max for Live add-on is required)";

    e.userLibrary = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Ableton").getChildFile ("User Library");
    e.userLibraryExists = e.userLibrary.isDirectory();
    e.targetDir = e.userLibrary.getChildFile ("Presets").getChildFile ("MIDI Effects").getChildFile ("Max MIDI Effect");
    e.sourceDir = findSourceDir();
    e.sourceComplete = e.sourceDir.getChildFile (kDeviceFile).existsAsFile();
    const auto dst = e.targetDir.getChildFile (kDeviceFile);
    e.installed = dst.existsAsFile();
    e.upToDate = e.installed && e.sourceComplete && dst.hasIdenticalContentTo (e.sourceDir.getChildFile (kDeviceFile))
              && e.targetDir.getChildFile (kScriptFile).hasIdenticalContentTo (e.sourceDir.getChildFile (kScriptFile));
    return e;
}

BridgeInstaller::Outcome BridgeInstaller::install (const BridgeEnvironment& e, bool overwrite, juce::String& msg)
{
    if (! e.sourceComplete) { msg = "The bridge device is not bundled with this build of GLOBAL CONTROL."; return Outcome::Failed; }
    if (! e.userLibraryExists) { msg = "Ableton User Library not found at " + e.userLibrary.getFullPathName() + ". Open Live once, or copy the files by hand."; return Outcome::Failed; }
    if (e.upToDate) { msg = "The bridge in the User Library is already up to date."; return Outcome::AlreadyUpToDate; }
    juce::StringArray different;
    for (const auto* f : { kDeviceFile, kScriptFile }) {
        const auto dst = e.targetDir.getChildFile (f);
        if (dst.existsAsFile() && ! dst.hasIdenticalContentTo (e.sourceDir.getChildFile (f))) different.add (dst.getFullPathName());
    }
    if (! different.isEmpty() && ! overwrite) {
        msg = "These files already exist and are different:\n" + different.joinIntoString ("\n") + "\nReplace them?";
        return Outcome::NeedsConfirmation;
    }
    if (! e.targetDir.createDirectory()) { msg = "Cannot create " + e.targetDir.getFullPathName(); return Outcome::Failed; }
    for (const auto* f : { kDeviceFile, kScriptFile }) {
        const auto dst = e.targetDir.getChildFile (f);
        if (dst.existsAsFile() && ! dst.hasIdenticalContentTo (e.sourceDir.getChildFile (f)))
            dst.copyFileTo (dst.getSiblingFile (dst.getFileNameWithoutExtension() + " (backup)" + dst.getFileExtension()));
        if (! e.sourceDir.getChildFile (f).copyFileTo (dst)) { msg = "Cannot copy " + juce::String (f) + " to " + e.targetDir.getFullPathName(); return Outcome::Failed; }
    }
    msg = "Installed in " + e.targetDir.getFullPathName();
    return Outcome::Installed;
}

} // namespace gc
