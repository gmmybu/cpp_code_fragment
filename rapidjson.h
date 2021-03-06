#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

using json_buffer = rapidjson::StringBuffer;

[[noreturn]] inline void json_throw_error()
{
    throw std::runtime_error("json parse error");
}

inline void json_guarantee(bool condition)
{
    if (condition) return;
    json_throw_error();
}

class json_value
{
public:
    json_value(const rapidjson::Value& v) : _value(v)
    {
    }

    bool has_key(const char* key) const
    {
        json_guarantee(_value.IsObject());
        return _value.HasMember(key);
    }

    json_value operator[](const char* key) const
    {
        json_guarantee(_value.IsObject());

        auto iter = _value.FindMember(key);
        json_guarantee(iter != _value.MemberEnd());
        return iter->value;
    }

    json_value operator[](size_t index) const
    {
        json_guarantee(_value.IsArray());

        if (index >= _value.Size())
            throw std::runtime_error("json array out of range");

        return _value[index];
    }

    rapidjson::Value::ConstArray get_array() const
    {
        json_guarantee(_value.IsArray());
        return _value.GetArray();
    }

    template<class Func>
    void for_each(const Func& func)
    {
        for (auto& item : get_array()) {
            func(item);
        }
    }

    rapidjson::Value::ConstObject get_object() const
    {
        json_guarantee(_value.IsObject());
        return _value.GetObject();
    }

    std::string as_string() const
    {
        json_buffer buffer;
        rapidjson::Writer<json_buffer> writer{buffer};
        _value.Accept(writer);
        return {buffer.GetString(), buffer.GetLength()};
    }

    template<class T>
    operator T() const
    {
        T t; *this >> t;
        return t;
    }
public:
    const rapidjson::Value& get_value() const
    {
        return _value;
    }
private:
    const rapidjson::Value& _value;
};

class json_document
{
public:
    void parse(const char* str)
    {
        _doc.Parse(str);
    }

    void parse(const char* str, size_t size)
    {
        _doc.Parse(str, size);
    }

    void parse(const std::string& str)
    {
        _doc.Parse(str.c_str(), str.size());
    }

    const rapidjson::Document& get_document() const
    {
        return _doc;
    }

    json_value operator[](const char* key) const
    {
        return json_value{_doc}[key] ;
    }

    operator json_value()
    {
        return _doc;
    }
private:
    rapidjson::Document _doc;
public:
    template<class T>
    friend void operator>>(const json_document& doc, T& t)
    {
        json_value{doc._doc} >> t;
    }
};

inline void operator>>(json_value json, uint32_t& x)
{
    auto& v = json.get_value();
    if (v.IsUint()) { x = v.GetUint(); return; }
    if (v.IsNull()) { x = 0;           return; }

    if (v.IsString()) {
        if (sscanf_s(v.GetString(), "%u", &x) == 1)
            return;
    }

    json_throw_error();
}

inline void operator>>(json_value json, int32_t& x)
{
    auto& v = json.get_value();
    if (v.IsInt())  { x = v.GetInt(); return; }
    if (v.IsNull()) { x = 0;          return; }

    if (v.IsString()) {
        if (sscanf_s(v.GetString(), "%d", &x) == 1)
            return;
    }

    json_throw_error();
}

inline void operator>>(json_value json, uint64_t& x)
{
    auto& v = json.get_value();
    if (v.IsUint64()) { x = v.GetUint64(); return; }
    if (v.IsNull())   { x = 0;             return; }

    if (v.IsString()) {
        if (sscanf_s(v.GetString(), "%I64u", &x) == 1)
            return;
    }

    json_throw_error();
}

inline void operator>>(json_value json, float& x)
{
    auto& v = json.get_value();
    if (v.IsNumber()) { x = v.GetFloat(); return; }
    if (v.IsNull())   { x = 0.f;          return; }

    if (v.IsString()) {
        if (sscanf_s(v.GetString(), "%f", &x) == 1)
            return;
    }

    json_throw_error();
}

inline void operator>>(json_value json, double& x)
{
    auto& v = json.get_value();
    if (v.IsNumber()) { x = v.GetDouble(); return; }
    if (v.IsNull())   { x = 0.0;           return; }
    
    if (v.IsString()) {
        if (sscanf_s(v.GetString(), "%lf", &x) == 1)
            return;
    }

    json_throw_error();
}

inline void operator>>(json_value json, bool& x)
{
    auto& v = json.get_value();
    if (v.IsBool()) {
        x = v.GetBool();
    } else {
        json_throw_error();
    }
}

inline void operator>>(json_value json, std::string& text)
{
    auto& v = json.get_value();
    if (v.IsString()) { text = v.GetString(); return; }
    if (v.IsNull())   { text.clear();         return; }

    text = json.as_string();
}

template<class T>
void operator>>(json_value json, std::vector<T>& data)
{
    if (json.get_value().IsObject()) {
        json["list"] >> data;
    } else {
        for (auto& item : json.get_array()) {
            T val; json_value{item} >> val;
            data.push_back(std::move(val));
        }
    }
}

class json_writer
{
public:
    json_writer(json_buffer& buffer) :
        _writer(buffer)
    {
    }

    rapidjson::Writer<json_buffer>& get_writer()
    {
        return _writer;
    }

    void array(const std::string& value)
    {
        if (value.size() > 0) {
            _writer.RawValue(value.c_str(), value.size(),
                rapidjson::kArrayType);
        } else {
            _writer.RawValue("[]", 2, rapidjson::kArrayType);
        }
    }

    void object(const std::string& value)
    {
        if (value.size() > 0) {
            _writer.RawValue(value.c_str(), value.size(),
                rapidjson::kObjectType);
        } else {
            _writer.RawValue("{}", 2, rapidjson::kObjectType);
        }
    }
private:
    rapidjson::Writer<json_buffer> _writer;
};

class json_object_writer
{
public:
    json_object_writer(json_writer& writer) :
        _writer(writer)
    {
        _writer.get_writer().StartObject();
    }

    ~json_object_writer()
    {
        _writer.get_writer().EndObject();
    }

    json_writer& operator[](const char* key)
    {
        _writer.get_writer().Key(key);
        return _writer;
    }
private:
    json_writer& _writer;
private:
    json_object_writer(const json_object_writer&) = delete;
    json_object_writer& operator=(const json_object_writer&) = delete;
};

class json_array_writer
{
public:
    json_array_writer(json_writer& writer) :
        _writer(writer)
    {
        _writer.get_writer().StartArray();
    }

    ~json_array_writer()
    {
        _writer.get_writer().EndArray();
    }

    operator json_writer&()
    {
        return _writer;
    }
private:
    json_writer& _writer;
private:
    json_array_writer(const json_array_writer&) = delete;
    json_array_writer& operator=(const json_array_writer&) = delete;
};

inline void operator<<(json_writer& writer, const std::string& val)
{
    writer.get_writer().String(val.c_str(), val.size());
}

inline void operator<<(json_writer& writer, const char* val)
{
    writer.get_writer().String(val);
}

inline void operator<<(json_writer& writer, float num)
{
    char num_text[32] = {};
    sprintf_s(num_text, "%.2f", num);
    
    writer.get_writer().String(num_text);
}

inline void operator<<(json_writer& writer, const json_buffer& buffer)
{
    writer.get_writer().String(buffer.GetString());
}

inline void operator<<(json_writer& writer, const json_value& val)
{
    std::string str = val;
    writer << str;
}

template<bool>
struct json_write_redirector
{
    template<class T>
    static void write(json_writer& w, const T& t)
    {
        json_object_writer oo{w};
        oo << t;
    }
};

template<>
struct json_write_redirector<true>
{
    template<class T>
    static void write(json_writer& w, const T& t)
    {
        w << std::to_string(t);
    }
};

template<class T>
void operator<<(json_writer& w, const T& t)
{
    json_write_redirector<std::is_fundamental_v<T>>::write(w, t);
}

template<class Func>
json_buffer json_serialize(const Func& func)
{
    json_buffer buffer;
    json_writer writer{buffer};
    func(writer);
    return buffer;
}

template<class T>
void operator<<(json_writer& writer, const std::vector<T>& val)
{
    json_array_writer aa{writer};
    for (auto& v : val) {
        aa << v;
    }
}

template<class T>
void operator<<(json_writer& writer, const std::vector<T*>& val)
{
    json_array_writer aa{writer};
    for (auto v : val) {
        aa << *v;
    }
}
