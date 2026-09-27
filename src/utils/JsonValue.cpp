#include "utils/JsonValue.h"

#include <charconv>
#include <cctype>
#include <cstdlib>

namespace
{
    class JsonParser
    {
    public:
        explicit JsonParser(std::string_view source) : source_(source) {}

        std::optional<JsonValue> parse(std::string* error)
        {
            skipWhitespace();
            auto value = parseValue(0);
            skipWhitespace();
            if (!value || position_ != source_.size())
            {
                if (error != nullptr)
                {
                    *error = error_.empty()
                        ? "Conteúdo adicional após o valor JSON."
                        : error_;
                }
                return std::nullopt;
            }
            return value;
        }

    private:
        std::optional<JsonValue> parseValue(int depth)
        {
            if (depth > 128 || position_ >= source_.size())
            {
                return fail("Estrutura JSON inválida ou profunda demais.");
            }

            switch (source_[position_])
            {
            case '{': return parseObject(depth + 1);
            case '[': return parseArray(depth + 1);
            case '"':
            {
                auto text = parseString();
                return text ? std::optional<JsonValue>(JsonValue(std::move(*text)))
                            : std::nullopt;
            }
            case 't': return parseLiteral("true", JsonValue(true));
            case 'f': return parseLiteral("false", JsonValue(false));
            case 'n': return parseLiteral("null", JsonValue());
            default: return parseNumber();
            }
        }

        std::optional<JsonValue> parseObject(int depth)
        {
            ++position_;
            skipWhitespace();
            JsonValue::Object object;
            if (consume('}'))
            {
                return JsonValue(std::move(object));
            }

            while (position_ < source_.size())
            {
                auto key = parseString();
                if (!key)
                {
                    return std::nullopt;
                }
                skipWhitespace();
                if (!consume(':'))
                {
                    return fail("Esperado ':' depois da chave JSON.");
                }
                skipWhitespace();
                auto value = parseValue(depth);
                if (!value)
                {
                    return std::nullopt;
                }
                object.insert_or_assign(std::move(*key), std::move(*value));
                skipWhitespace();
                if (consume('}'))
                {
                    return JsonValue(std::move(object));
                }
                if (!consume(','))
                {
                    return fail("Esperado ',' entre propriedades JSON.");
                }
                skipWhitespace();
            }
            return fail("Objeto JSON não foi encerrado.");
        }

        std::optional<JsonValue> parseArray(int depth)
        {
            ++position_;
            skipWhitespace();
            JsonValue::Array array;
            if (consume(']'))
            {
                return JsonValue(std::move(array));
            }

            while (position_ < source_.size())
            {
                auto value = parseValue(depth);
                if (!value)
                {
                    return std::nullopt;
                }
                array.push_back(std::move(*value));
                skipWhitespace();
                if (consume(']'))
                {
                    return JsonValue(std::move(array));
                }
                if (!consume(','))
                {
                    return fail("Esperado ',' entre itens JSON.");
                }
                skipWhitespace();
            }
            return fail("Array JSON não foi encerrado.");
        }

        std::optional<std::string> parseString()
        {
            if (!consume('"'))
            {
                fail("Esperada uma string JSON.");
                return std::nullopt;
            }

            std::string result;
            while (position_ < source_.size())
            {
                const unsigned char character = source_[position_++];
                if (character == '"')
                {
                    return result;
                }
                if (character < 0x20)
                {
                    fail("Caractere de controle dentro de string JSON.");
                    return std::nullopt;
                }
                if (character != '\\')
                {
                    result += static_cast<char>(character);
                    continue;
                }
                if (position_ >= source_.size())
                {
                    break;
                }

                const char escaped = source_[position_++];
                switch (escaped)
                {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u':
                {
                    const auto codePoint = parseHexCodePoint();
                    if (!codePoint)
                    {
                        return std::nullopt;
                    }
                    appendUtf8(result, *codePoint);
                    break;
                }
                default:
                    fail("Escape desconhecido em string JSON.");
                    return std::nullopt;
                }
            }
            fail("String JSON não foi encerrada.");
            return std::nullopt;
        }

        std::optional<unsigned> parseHexCodePoint()
        {
            if (position_ + 4 > source_.size())
            {
                fail("Escape Unicode JSON incompleto.");
                return std::nullopt;
            }
            unsigned value = 0;
            for (int index = 0; index < 4; ++index)
            {
                const char digit = source_[position_++];
                value *= 16;
                if (digit >= '0' && digit <= '9') value += digit - '0';
                else if (digit >= 'a' && digit <= 'f') value += digit - 'a' + 10;
                else if (digit >= 'A' && digit <= 'F') value += digit - 'A' + 10;
                else
                {
                    fail("Escape Unicode JSON inválido.");
                    return std::nullopt;
                }
            }
            return value;
        }

        static void appendUtf8(std::string& output, unsigned codePoint)
        {
            if (codePoint <= 0x7F)
            {
                output += static_cast<char>(codePoint);
            }
            else if (codePoint <= 0x7FF)
            {
                output += static_cast<char>(0xC0 | (codePoint >> 6));
                output += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            else
            {
                output += static_cast<char>(0xE0 | (codePoint >> 12));
                output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                output += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
        }

        std::optional<JsonValue> parseNumber()
        {
            const std::size_t start = position_;
            if (position_ < source_.size() && source_[position_] == '-') ++position_;
            while (position_ < source_.size() && std::isdigit(
                       static_cast<unsigned char>(source_[position_]))) ++position_;
            if (position_ < source_.size() && source_[position_] == '.')
            {
                ++position_;
                while (position_ < source_.size() && std::isdigit(
                           static_cast<unsigned char>(source_[position_]))) ++position_;
            }
            if (position_ < source_.size() &&
                (source_[position_] == 'e' || source_[position_] == 'E'))
            {
                ++position_;
                if (position_ < source_.size() &&
                    (source_[position_] == '+' || source_[position_] == '-')) ++position_;
                while (position_ < source_.size() && std::isdigit(
                           static_cast<unsigned char>(source_[position_]))) ++position_;
            }
            if (position_ == start)
            {
                return fail("Valor JSON desconhecido.");
            }

            const std::string text(source_.substr(start, position_ - start));
            char* end = nullptr;
            const double value = std::strtod(text.c_str(), &end);
            if (end == nullptr || *end != '\0')
            {
                return fail("Número JSON inválido.");
            }
            return JsonValue(value);
        }

        std::optional<JsonValue> parseLiteral(std::string_view literal, JsonValue value)
        {
            if (source_.substr(position_, literal.size()) != literal)
            {
                return fail("Literal JSON inválido.");
            }
            position_ += literal.size();
            return value;
        }

        std::optional<JsonValue> fail(std::string message)
        {
            if (error_.empty())
            {
                error_ = std::move(message);
            }
            return std::nullopt;
        }

        bool consume(char expected)
        {
            if (position_ < source_.size() && source_[position_] == expected)
            {
                ++position_;
                return true;
            }
            return false;
        }

        void skipWhitespace()
        {
            while (position_ < source_.size() && std::isspace(
                       static_cast<unsigned char>(source_[position_]))) ++position_;
        }

        std::string_view source_;
        std::size_t position_ = 0;
        std::string error_;
    };
}

JsonValue::JsonValue() = default;
JsonValue::JsonValue(bool value) : value_(value) {}
JsonValue::JsonValue(double value) : value_(value) {}
JsonValue::JsonValue(std::string value) : value_(std::move(value)) {}
JsonValue::JsonValue(Array value) : value_(std::move(value)) {}
JsonValue::JsonValue(Object value) : value_(std::move(value)) {}

std::optional<JsonValue> JsonValue::parse(std::string_view json, std::string* error)
{
    return JsonParser(json).parse(error);
}

const JsonValue* JsonValue::find(std::string_view key) const
{
    const auto* object = objectValue();
    if (object == nullptr) return nullptr;
    const auto iterator = object->find(key);
    return iterator == object->end() ? nullptr : &iterator->second;
}

const std::string* JsonValue::stringValue() const
{
    return std::get_if<std::string>(&value_);
}

const JsonValue::Array* JsonValue::arrayValue() const
{
    return std::get_if<Array>(&value_);
}

const JsonValue::Object* JsonValue::objectValue() const
{
    return std::get_if<Object>(&value_);
}

bool JsonValue::boolValue(bool fallback) const
{
    const auto* value = std::get_if<bool>(&value_);
    return value == nullptr ? fallback : *value;
}

double JsonValue::numberValue(double fallback) const
{
    const auto* value = std::get_if<double>(&value_);
    return value == nullptr ? fallback : *value;
}
