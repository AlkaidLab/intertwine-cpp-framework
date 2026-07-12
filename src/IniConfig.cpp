#include "intertwine/fw/IniConfig.hpp"
#include <hv/iniparser.h>
#include <list>

namespace intertwine {
namespace fw {

IniConfig::IniConfig() : m_impl(new IniParser()) {}

IniConfig::~IniConfig() {
    delete static_cast<IniParser*>(m_impl);
}

static IniParser& parser(void* p) {
    return *static_cast<IniParser*>(p);
}

/* -- Load / Save -- */

bool IniConfig::loadFromFile(const char* filename) {
    return parser(m_impl).LoadFromFile(filename) == 0;
}

bool IniConfig::loadFromMem(const char* content) {
    return parser(m_impl).LoadFromMem(content) == 0;
}

bool IniConfig::save() const {
    return parser(m_impl).Save() == 0;
}

bool IniConfig::saveAs(const char* filename) const {
    return parser(m_impl).SaveAs(filename) == 0;
}

void IniConfig::unload() {
    parser(m_impl).Unload();
}

/* -- Read -- */

std::string IniConfig::getString(const char* key, const char* section) const {
    return parser(m_impl).GetValue(key, section);
}

int IniConfig::getInt(const char* key, const char* section, int def) const {
    return parser(m_impl).Get<int>(key, section, def);
}

bool IniConfig::getBool(const char* key, const char* section, bool def) const {
    return parser(m_impl).Get<bool>(key, section, def);
}

float IniConfig::getFloat(const char* key, const char* section, float def) const {
    return parser(m_impl).Get<float>(key, section, def);
}

/* -- Write -- */

void IniConfig::setString(const char* key, const std::string& val, const char* section) {
    parser(m_impl).SetValue(key, val, section);
}

void IniConfig::setInt(const char* key, int val, const char* section) {
    parser(m_impl).Set<int>(key, val, section);
}

void IniConfig::setBool(const char* key, bool val, const char* section) {
    parser(m_impl).Set<bool>(key, val, section);
}

void IniConfig::setFloat(const char* key, float val, const char* section) {
    parser(m_impl).Set<float>(key, val, section);
}

/* -- Enumeration -- */

std::vector<std::string> IniConfig::sections() const {
    std::list<std::string> lst = parser(m_impl).GetSections();
    return std::vector<std::string>(lst.begin(), lst.end());
}

std::vector<std::string> IniConfig::keys(const char* section) const {
    std::list<std::string> lst = parser(m_impl).GetKeys(section);
    return std::vector<std::string>(lst.begin(), lst.end());
}

} // namespace fw
} // namespace intertwine
