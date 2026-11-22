#include "NovaLicense.h"

#include <juce_product_unlocking/juce_product_unlocking.h>
#include <juce_cryptography/juce_cryptography.h>

namespace
{
    // XOR-obfuscates `data` in place against a repeating SHA256 keystream
    // derived from `key`. Symmetric (same function encrypts and decrypts).
    void xorObfuscate (juce::MemoryBlock& data, const juce::String& key)
    {
        const juce::SHA256 hash (key.toUTF8());
        const auto digestBlock = hash.getRawData(); // 32 bytes
        const auto* digest = (const uint8_t*) digestBlock.getData();
        const size_t digestSize = digestBlock.getSize();

        auto* bytes = (uint8_t*) data.getData();
        for (size_t i = 0; i < data.getSize(); ++i)
            bytes[i] = (uint8_t) (bytes[i] ^ digest[i % digestSize]);
    }
}

struct NovaLicense::ActivationThread : public juce::Thread
{
    ActivationThread (NovaLicense& ownerIn, juce::String keyIn, ActivationCallback callbackIn)
        : juce::Thread ("NovaLicenseActivation"),
          owner (ownerIn), licenseKey (std::move (keyIn)), callback (std::move (callbackIn))
    {
    }

    void run() override
    {
        const auto machineId = owner.getMachineId();

        juce::DynamicObject::Ptr payload = new juce::DynamicObject();
        payload->setProperty ("licenseKey", licenseKey);
        payload->setProperty ("machineId", machineId);
        const auto body = juce::JSON::toString (juce::var (payload.get()), true);

        juce::URL url (juce::String (NOVA_LICENSE_SERVER_URL) + "/api/activate");
        url = url.withPOSTData (body);

        const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                  .withExtraHeaders ("Content-Type: application/json")
                                  .withConnectionTimeoutMs (8000);

        int statusCode = 0;
        juce::String responseBody;

        if (auto stream = url.createInputStream (options.withStatusCode (&statusCode)))
            responseBody = stream->readEntireStreamAsString();

        ActivationResult result = ActivationResult::NetworkError;

        if (statusCode > 0)
        {
            const auto json = juce::JSON::parse (responseBody);
            if (auto* obj = json.getDynamicObject())
            {
                const bool ok = (bool) obj->getProperty ("ok");
                if (ok)
                {
                    if (owner.writeCache (licenseKey, machineId))
                        result = ActivationResult::Success;
                    else
                        result = ActivationResult::InvalidResponse;
                }
                else
                {
                    const auto reason = obj->getProperty ("reason").toString();
                    if (reason == "invalid_key")            result = ActivationResult::InvalidKey;
                    else if (reason == "revoked")            result = ActivationResult::Revoked;
                    else if (reason == "seat_limit_reached")  result = ActivationResult::SeatLimitReached;
                    else                                       result = ActivationResult::InvalidResponse;
                }
            }
            else
            {
                result = ActivationResult::InvalidResponse;
            }
        }

        juce::MessageManager::callAsync ([callback = callback, result]
        {
            if (callback)
                callback (result);
        });
    }

    NovaLicense& owner;
    juce::String licenseKey;
    ActivationCallback callback;
};

NovaLicense::NovaLicense (juce::String appIdIn) : appId (std::move (appIdIn))
{
}

NovaLicense::~NovaLicense()
{
    // Give the background activation call a chance to finish cleanly
    // rather than destroying `this` out from under a thread that still
    // holds a reference to it. Matches (slightly exceeds) the network
    // call's own connection timeout below so this always waits long
    // enough for the thread to actually reach its callAsync and return.
    if (activeThread != nullptr)
        activeThread->stopThread (9000);
}

juce::File NovaLicense::getCacheFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("International Nova")
        .getChildFile (appId + ".novalicense");
}

juce::String NovaLicense::getMachineId() const
{
    return juce::OnlineUnlockStatus::MachineIDUtilities::getUniqueMachineID();
}

juce::String NovaLicense::getObfuscationKey (const juce::String& machineId) const
{
    // Not a secret in the "must never leak" sense - this only needs to
    // deter casually editing the cache file by hand. Combines a fixed
    // per-product salt with the machine ID so the cache can't just be
    // copied verbatim to another machine and pass the check.
    return "NovaLicense::" + appId + "::" + machineId;
}

bool NovaLicense::writeCache (const juce::String& licenseKey, const juce::String& machineId) const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("licenseKey", licenseKey);
    obj->setProperty ("machineId", machineId);
    obj->setProperty ("activatedAt", juce::Time::getCurrentTime().toISO8601 (true));

    const auto plainText = juce::JSON::toString (juce::var (obj.get()), true);
    juce::MemoryBlock block (plainText.toUTF8(), plainText.getNumBytesAsUTF8());
    xorObfuscate (block, getObfuscationKey (machineId));

    const auto file = getCacheFile();
    if (! file.getParentDirectory().createDirectory())
        return false;

    return file.replaceWithData (block.getData(), block.getSize());
}

bool NovaLicense::readCache (juce::String& outLicenseKey, juce::String& outMachineId) const
{
    const auto file = getCacheFile();
    if (! file.existsAsFile())
        return false;

    juce::MemoryBlock block;
    if (! file.loadFileAsData (block) || block.getSize() == 0)
        return false;

    // The obfuscation key depends on the *current* machine ID, so this
    // only decrypts to valid JSON if the cache was written on this same
    // machine - moving the file to another machine yields garbage, which
    // JSON::parse below simply fails on.
    const auto currentMachineId = getMachineId();
    xorObfuscate (block, getObfuscationKey (currentMachineId));

    const auto text = juce::String::fromUTF8 ((const char*) block.getData(), (int) block.getSize());
    const auto json = juce::JSON::parse (text);
    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
        return false;

    outLicenseKey = obj->getProperty ("licenseKey").toString();
    outMachineId = obj->getProperty ("machineId").toString();
    return outLicenseKey.isNotEmpty() && outMachineId == currentMachineId;
}

bool NovaLicense::isUnlocked() const
{
    juce::String licenseKey, machineId;
    return readCache (licenseKey, machineId);
}

void NovaLicense::activate (const juce::String& licenseKey, ActivationCallback callback)
{
    activeThread = std::make_unique<ActivationThread> (*this, licenseKey.trim().toUpperCase(), std::move (callback));
    activeThread->startThread();
}
