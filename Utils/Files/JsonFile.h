//
// Created by y1 on 2025-09-09.
//

#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include <simdjson.h>

#include "Debug.h"
#include "FileSystem.h"

namespace drez::file_system::json_helper {
template<typename... Args>
void DebugCheckCriticalSimdJson(const simdjson::error_code error, const spdlog::format_string_t<Args..., const char *> &failMessage, Args &&...args) {
    DebugCheckCritical(error == simdjson::SUCCESS, failMessage, std::forward<Args>(args)..., simdjson::error_message(error));
}

// simdjson::error_code JsonConvert(simdjson::dom::element, glm::vec3 &);
simdjson::error_code JsonConvert(simdjson::dom::element, std::string &);
simdjson::error_code JsonConvert(simdjson::dom::element, double &);
simdjson::error_code JsonConvert(simdjson::dom::element, float &);
simdjson::error_code JsonConvert(simdjson::dom::element, bool &);
simdjson::error_code JsonConvert(simdjson::dom::element, int &);
template<typename T>
simdjson::error_code JsonConvert(simdjson::dom::element, T &);
template<typename T>
simdjson::error_code JsonConvert(simdjson::dom::element, std::vector<T> &);

template<typename T>
simdjson::error_code JsonGet(simdjson::dom::object &obj, std::string_view key, T &value);

template<typename T>
simdjson::error_code JsonConvert(simdjson::dom::element element, T &out) {
    return element.get(out);
}

inline simdjson::error_code JsonConvert(simdjson::dom::element element, double &out) {
    auto result = element.get_double();

    if (!result.error()) {
        out = result.value();
    }

    return result.error();
}

inline simdjson::error_code JsonConvert(simdjson::dom::element element, float &out) {
    auto result = element.get_double();

    if (!result.error()) {
        out = static_cast<float>(result.value());
    }

    return result.error();
}

inline simdjson::error_code JsonConvert(simdjson::dom::element element, bool &out) {
    auto result = element.get_bool();

    if (!result.error()) {
        out = result.value();
    }
    return result.error();
}

inline simdjson::error_code JsonConvert(simdjson::dom::element element, int &out) {
    auto result = element.get_int64();

    if (!result.error()) {
        out = static_cast<int>(result.value());
    }
    return result.error();
}

inline simdjson::error_code JsonConvert(simdjson::dom::element element, std::string &out) {
    auto result = element.get_string();

    if (!result.error()) {
        std::string_view sv = result.value();

        out.assign(sv.data(), sv.size());
    }

    return result.error();
}

inline simdjson::error_code JsonConvert(simdjson::dom::element element, std::string_view &out) {
    auto result = element.get_string();

    if (!result.error()) {
        out = result.value();
    }

    return result.error();
}

template<typename T>
simdjson::error_code JsonConvert(simdjson::dom::element element, std::vector<T> &out) {
    simdjson::dom::array arr;
    simdjson::error_code err = element.get(arr);

    if (err) {
        return err;
    }

    out.clear();
    out.reserve(arr.size());

    for (auto v: arr) {
        T value{};
        err = JsonConvert(v, value);
        if (err) {
            return err;
        }

        out.emplace_back(std::move(value));
    }
    return simdjson::SUCCESS;
}

template<typename T>
simdjson::error_code JsonGet(simdjson::dom::object &obj, std::string_view key, T &value) {
    simdjson::dom::element sub;
    simdjson::error_code   e = obj.at_key(key).get(sub);
    if (e) {
        return e;
    }
    return JsonConvert(sub, value);
}

inline void Indent(std::ostream &os, int depth) {
    for (int i = 0; i < depth; ++i) {
        os << "  ";
    }
}

inline void JsonWrite(std::ostream &os, const std::string &v, int) {
    os << "\"" << v << "\"";
}

inline void JsonWrite(std::ostream &os, float v, int) {
    os << v;
}

inline void JsonWrite(std::ostream &os, double v, int) {
    os << v;
}

inline void JsonWrite(std::ostream &os, int v, int) {
    os << v;
}

inline void JsonWrite(std::ostream &os, bool v, int) {
    os << (v ? "true" : "false");
}

template<typename T>
void JsonWrite(std::ostream &os, const std::vector<T> &items, int depth) {
    os << "[\n";
    for (size_t i = 0; i < items.size(); ++i) {
        Indent(os, depth + 1);
        JsonWrite(os, items[i], depth + 1);
        if (i + 1 < items.size()) os << ",";
        os << "\n";
    }
    Indent(os, depth);
    os << "]";
}

} // namespace drez::file_system::json_helper

class JsonFile {
public:
    explicit JsonFile(std::string_view file) {
        const simdjson::error_code error = m_parser.parse(drez::file_system::Read(file)).get(m_doc);

        drez::file_system::json_helper::DebugCheckCriticalSimdJson(error, "Failed to load {}", file);
    }

    JsonFile(simdjson::dom::element doc, bool valid)
        : m_doc(doc), m_valid(valid) {}

    JsonFile() = delete;

    ~JsonFile() = default;

    JsonFile(const JsonFile &)            = delete;
    JsonFile(JsonFile &&)                 = delete;
    JsonFile &operator=(const JsonFile &) = delete;
    JsonFile &operator=(JsonFile &&)      = delete;

    template<typename T>
    T Get(std::string_view key) const {
        return GetCriticalField<T>(key);
    }

    template<typename T>
    T Get(std::string_view key, const T &defaultValue) const {
        return GetField<T>(key, defaultValue);
    }

    JsonFile Field(std::string_view key) const {
        if (!m_valid) {
            return JsonFile(simdjson::dom::element{}, false);
        }
        simdjson::dom::element el;
        if (m_doc.at_key(key).get(el) != simdjson::SUCCESS) {
            return JsonFile(simdjson::dom::element{}, false);
        }
        return JsonFile(el, true);
    }

    template<typename T>
    static bool Write(const std::string &filePath, const T &value) {
        std::ostringstream os;
        drez::file_system::json_helper::JsonWrite(os, value, 0);
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        file << os.str();
        return file.good();
    }

private:
    simdjson::dom::parser  m_parser;
    simdjson::dom::element m_doc;
    bool                   m_valid{true};

    template<typename T>
    T GetField(std::string_view key, const T &defaultValue) const {
        if (!m_valid) {
            return defaultValue;
        }

        T                      value{};
        simdjson::dom::element element = m_doc;
        const auto             err     = m_doc.at_key(key).get(element);
        if (err != simdjson::SUCCESS) {
            return defaultValue;
        }

        const auto parseErr = drez::file_system::json_helper::JsonConvert(element, value);
        if (parseErr != simdjson::SUCCESS) {
            return defaultValue;
        }

        return value;
    }

    template<typename T>
    T GetCriticalField(std::string_view key) const {
        DebugCheckCritical(m_valid, "Cannot read critical field {} from invalid json view", key);

        T                      value{};
        simdjson::dom::element element;
        const auto             err = m_doc.at_key(key).get(element);
        drez::file_system::json_helper::DebugCheckCriticalSimdJson(err, "Failed to get critical field {}", key);

        const auto parseErr = drez::file_system::json_helper::JsonConvert(element, value);
        drez::file_system::json_helper::DebugCheckCriticalSimdJson(parseErr, "Failed to parse critical field {}", key);

        return value;
    }
};
