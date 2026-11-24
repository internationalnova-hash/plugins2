#pragma once

// Shared seat-limited license activation, used the same way by every Nova plugin.
//
// Two kinds of key:
//   - One-time ("perpetual"): bought for one plugin, unlocks that plugin forever.
//   - Nova All Access ("subscription"): unlocks every Nova plugin until it expires.
//     Activating it in any plugin unlocks them all on this computer (shared cache).
//     It's quietly re-checked online every few days (refreshIfDue) to pick up
//     renewals; offline it keeps working for kOfflineGraceDays past its expiry.
//
// Flow:
//   1. isUnlocked() (local cache only, no network) decides whether to show the
//      unlock screen. NovaLicenseGate (NovaLicenseGate.h) does all of this for you.
//   2. activate(key, callback) POSTs to NOVA_LICENSE_SERVER_URL "/api/activate" on a
//      background thread (with this plugin's id), then calls back on the message thread.
//   3. On success a local cache file is written, lightly obfuscated and tied to this
//      machine (deters casual tampering; the seat limit lives on the server).

#include <functional>

#include <juce_core/juce_core.h>

#ifndef NOVA_LICENSE_SERVER_URL
 #define NOVA_LICENSE_SERVER_URL "http://localhost:3000"
#endif

class NovaLicense
{
public:
    enum class ActivationResult
    {
        Success,
        InvalidKey,
        Revoked,
        SeatLimitReached,
        NetworkError,
        InvalidResponse,
        Expired,          // an All Access key whose subscription has ended
        WrongPlugin       // a one-time key for a different plugin
    };

    struct Status
    {
        bool unlocked = false;
        bool allAccess = false;          // unlocked by a Nova All Access subscription
        juce::String licenseKey;
        juce::Time expires;              // subscriptions only
    };

    static constexpr int kOfflineGraceDays = 7;   // subscription keeps working this long past expiry without a check
    static constexpr int kRecheckDays = 3;        // how often a subscription key is re-checked online

    // appId: this plugin's store id (e.g. "nova-tape"). Sent to the server so a
    // one-time key only unlocks the plugin it was bought for.
    explicit NovaLicense (juce::String appId);
    ~NovaLicense();

    // Local cache only - no network call.
    bool isUnlocked() const;
    Status getStatus() const;

    using ActivationCallback = std::function<void (ActivationResult)>;

    // Network call on a background thread; `callback` runs on the message thread.
    void activate (const juce::String& licenseKey, ActivationCallback callback);

    // If a subscription key is due for its re-check, confirm it online in the
    // background. `onChanged` (message thread) fires if the unlock state changed.
    void refreshIfDue (std::function<void()> onChanged = {});

    const juce::String& getAppId() const noexcept { return appId; }

private:
    struct CacheEntry
    {
        juce::String licenseKey, machineId, kind, plugin;
        juce::Time expires, lastCheck;
        bool valid = false;
    };

    juce::String appId;

    static juce::File cacheFileFor (const juce::String& id);
    juce::String getMachineId() const;
    static juce::String obfuscationKey (const juce::String& id, const juce::String& machineId);

    bool writeCache (const juce::String& id, const CacheEntry&) const;
    CacheEntry readCache (const juce::String& id) const;
    static bool entryUnlocks (const CacheEntry&);

    struct ActivationThread;
    struct CheckThread;
    std::unique_ptr<juce::Thread> activeThread, checkThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaLicense)
};
