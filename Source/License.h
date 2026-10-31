#pragma once
#include <JuceHeader.h>

// Licencia por usuario de TMU.
// El numero de serie es la firma RSA del mail del comprador. TMU solo tiene la clave publica:
// puede comprobar un serial, pero no fabricarlo. Los seriales se generan con
// "Generador de seriales TMU.html" (clave privada, NO se sube a GitHub ni a la web).
namespace tmulicense
{
inline const char* publicKey() { return "10001,b2f722fe69ff7f08fc8393ab18501eec312853eba13e56d075acfba4e84f4de88829aabf882a60fa09e447fbb89e2e068c9916383da7a4121c7341d0840c4952a5942afd6300b5b0b7de0333a627497d"; }

inline juce::String normaliseEmail (const juce::String& e) { return e.trim().toLowerCase(); }

inline juce::String cleanSerial (const juce::String& s)
{
    return s.toLowerCase().retainCharacters ("0123456789abcdef");
}

inline bool verify (const juce::String& email, const juce::String& serial)
{
    const auto mail = normaliseEmail (email);
    const auto hex = cleanSerial (serial);
    if (mail.isEmpty() || ! mail.containsChar ('@') || hex.length() < 32) return false;

    juce::BigInteger sig;
    sig.parseString (hex, 16);
    juce::RSAKey key (publicKey());
    if (! key.isValid() || ! key.applyToValue (sig)) return false;

    const auto msg = mail + "|TMU1";
    const auto digest = juce::SHA256 (msg.toRawUTF8(), msg.getNumBytesAsUTF8()).toHexString();
    juce::BigInteger expected;
    expected.parseString (digest, 16);
    return sig == expected;
}

// Activacion online (una sola vez por compu): el servidor de viszeversza.shop
// cuenta en cuantas compus se activo cada licencia y frena las que se comparten.
#ifndef TMU_ACTIVATION_URL
 #define TMU_ACTIVATION_URL "https://viszeversza.shop/api/activar"
#endif
inline const char* activationURL() { return TMU_ACTIVATION_URL; }

enum class OnlineResult { ok, limit, rejected, network };

inline juce::String deviceID()
{
    auto id = juce::SystemStats::getUniqueDeviceID();
    if (id.isEmpty()) id = juce::SystemStats::getComputerName() + juce::SystemStats::getFullUserName();
    return juce::SHA256 (id.toRawUTF8(), id.getNumBytesAsUTF8()).toHexString().substring (0, 40);
}

// Bloquea mientras espera la respuesta: llamarla desde un hilo aparte
inline OnlineResult activateOnline (const juce::String& email, const juce::String& serial)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("email", normaliseEmail (email));
    o->setProperty ("serial", cleanSerial (serial));
    o->setProperty ("device", deviceID());
    o->setProperty ("product", "tmu");

    int status = 0;
    auto url = juce::URL (activationURL()).withPOSTData (juce::JSON::toString (juce::var (o.get()), true));
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders ("Content-Type: application/json")
                    .withConnectionTimeoutMs (12000)
                    .withStatusCode (&status);
    auto stream = url.createInputStream (opts);
    if (stream == nullptr) return OnlineResult::network;

    const auto reply = juce::JSON::parse (stream->readEntireStreamAsString());
    if ((bool) reply.getProperty ("ok", false)) return OnlineResult::ok;
    if (reply.getProperty ("error", {}).toString() == "limite") return OnlineResult::limit;
    if (status == 403 || reply.getProperty ("error", {}).toString() == "serial") return OnlineResult::rejected;
    return OnlineResult::network;
}

inline juce::File licenseFile()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/TMU/TMU-licencia.xml");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("TMU/TMU-licencia.xml");
   #endif
}

// Estado compartido por todas las instancias de TMU abiertas en el proyecto
class State
{
public:
    State()
    {
        if (auto xml = juce::XmlDocument::parse (licenseFile()))
        {
            const auto m = xml->getStringAttribute ("email"), s = xml->getStringAttribute ("serial");
            if (verify (m, s)) { email = normaliseEmail (m); active = true; }
        }
    }

    bool isActive() const { return active.load(); }
    juce::String getEmail() const { const juce::ScopedLock sl (lock); return email; }

    bool activate (const juce::String& m, const juce::String& s)
    {
        if (! verify (m, s)) return false;
        juce::XmlElement xml ("TMULicense");
        xml.setAttribute ("email", normaliseEmail (m));
        xml.setAttribute ("serial", cleanSerial (s));
        auto f = licenseFile();
        f.getParentDirectory().createDirectory();
        xml.writeTo (f);
        { const juce::ScopedLock sl (lock); email = normaliseEmail (m); }
        active = true;
        return true;
    }

private:
    std::atomic<bool> active { false };
    juce::String email;
    juce::CriticalSection lock;
};
} // namespace tmulicense
