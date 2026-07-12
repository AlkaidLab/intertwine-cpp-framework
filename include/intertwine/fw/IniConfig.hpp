#ifndef INTERTWINE_CPP_FRAMEWORK_INI_CONFIG_HPP
#define INTERTWINE_CPP_FRAMEWORK_INI_CONFIG_HPP

// INI configuration file parser abstraction.
// Wraps the underlying INI parser implementation so that consumers
// never depend on a specific library (e.g. libhv iniparser).

#include <string>
#include <vector>

namespace intertwine {
namespace fw {

class IniConfig {
public:
    IniConfig();
    ~IniConfig();

    IniConfig(const IniConfig&) = delete;
    IniConfig& operator=(const IniConfig&) = delete;

    /* -- Load / Save -- */

    /** Load from a file. Returns true on success. */
    bool loadFromFile(const char* filename);

    /** Load from an in-memory INI string. Returns true on success. */
    bool loadFromMem(const char* content);

    /** Save to the previously loaded file. Returns true on success. */
    bool save() const;

    /** Save to a specific file. Returns true on success. */
    bool saveAs(const char* filename) const;

    /** Unload / clear all data. */
    void unload();

    /* -- Read -- */

    /** Get a string value. Returns empty string if not found. */
    std::string getString(const char* key, const char* section = "") const;

    /** Get an integer value. */
    int getInt(const char* key, const char* section = "", int def = 0) const;

    /** Get a boolean value. */
    bool getBool(const char* key, const char* section = "", bool def = false) const;

    /** Get a float value. */
    float getFloat(const char* key, const char* section = "", float def = 0.0f) const;

    /* -- Write -- */

    void setString(const char* key, const std::string& val, const char* section = "");
    void setInt(const char* key, int val, const char* section = "");
    void setBool(const char* key, bool val, const char* section = "");
    void setFloat(const char* key, float val, const char* section = "");

    /* -- Enumeration -- */

    /** Return all section names. */
    std::vector<std::string> sections() const;

    /** Return all keys under a given section (empty = root). */
    std::vector<std::string> keys(const char* section = "") const;

private:
    void* m_impl; // opaque pointer to hide the underlying parser
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_INI_CONFIG_HPP
