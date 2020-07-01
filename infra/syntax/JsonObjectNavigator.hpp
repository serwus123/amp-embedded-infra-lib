#ifndef JSON_OBJECT_NAVIGATOR_HPP
#define JSON_OBJECT_NAVIGATOR_HPP

#include "infra/syntax/Json.hpp"
#include "infra/util/Optional.hpp"
#include <functional>

namespace infra
{
    class JsonObjectNavigator;
    class JsonOptionalObjectNavigator;

    struct JsonObjectNavigatorToken
    {
        std::string name;
    };

    struct JsonOptionalObjectNavigatorToken
    {
        std::string name;
    };

    struct JsonStringNavigatorToken
    {
        std::string name;
    };

    struct JsonIntegerNavigatorToken
    {
        std::string name;
    };

    struct JsonBoolNavigatorToken
    {
        std::string name;
    };

    template<class Result>
    struct JsonTransformObjectNavigatorToken
    {
        std::string name;
        std::function<Result(const JsonObjectNavigator& navigator)> transformation;
    };

    template<class Result>
    struct JsonTransformOptionalObjectNavigatorToken
    {
        std::string name;
        std::function<Result(const JsonObjectNavigator& navigator)> transformation;
    };

    template<class Result>
    struct JsonTransformStringNavigatorToken
    {
        std::string name;
        std::function<Result(const std::string& value)> transformation;
    };

    template<class Result>
    struct JsonTransformOptionalStringNavigatorToken
    {
        std::string name;
        std::function<Result(const std::string& value)> transformation;
    };

    class JsonObjectNavigator
    {
    public:
        JsonObjectNavigator(const std::string& contents);
        JsonObjectNavigator(infra::JsonObject& object);

        JsonObjectNavigator operator/(JsonObjectNavigatorToken token) const;
        JsonOptionalObjectNavigator operator/(JsonOptionalObjectNavigatorToken token) const;
        std::string operator/(JsonStringNavigatorToken token) const;
        int32_t operator/(JsonIntegerNavigatorToken token) const;
        bool operator/(JsonBoolNavigatorToken token) const;

        template<class Result>
            Result operator/(JsonTransformObjectNavigatorToken<Result> token) const;
        template<class Result>
            infra::Optional<Result> operator/(JsonTransformOptionalObjectNavigatorToken<Result> token) const;
        template<class Result>
            Result operator/(JsonTransformStringNavigatorToken<Result> token) const;
        template<class Result>
            infra::Optional<Result> operator/(JsonTransformOptionalStringNavigatorToken<Result> token) const;

    protected:
        mutable infra::JsonObject object;
    };

    class JsonOptionalObjectNavigator
    {
    public:
        JsonOptionalObjectNavigator() = default;
        JsonOptionalObjectNavigator(infra::JsonObject& object);
        JsonOptionalObjectNavigator(const JsonObjectNavigator& navigator);

        JsonOptionalObjectNavigator operator/(JsonObjectNavigatorToken token) const;
        JsonOptionalObjectNavigator operator/(JsonOptionalObjectNavigatorToken token) const;
        infra::Optional<std::string> operator/(JsonStringNavigatorToken token) const;
        infra::Optional<int32_t> operator/(JsonIntegerNavigatorToken token) const;
        infra::Optional<bool> operator/(JsonBoolNavigatorToken token) const;

        template<class Result>
            infra::Optional<Result> operator/(JsonTransformObjectNavigatorToken<Result> token) const;
        template<class Result>
            infra::Optional<Result> operator/(JsonTransformOptionalObjectNavigatorToken<Result> token) const;
        template<class Result>
            infra::Optional<Result> operator/(JsonTransformStringNavigatorToken<Result> token) const;
        template<class Result>
            infra::Optional<Result> operator/(JsonTransformOptionalStringNavigatorToken<Result> token) const;

    protected:
        infra::Optional<JsonObjectNavigator> navigator;
    };

    //// Implementation    ////

    template<class Result>
    Result JsonObjectNavigator::operator/(JsonTransformObjectNavigatorToken<Result> token) const
    {
        auto subObject = object.GetOptionalObject(token.name);
        if (subObject == infra::none)
            throw std::runtime_error(("Object " + token.name + " not found").c_str());

        return token.transformation(*subObject);
    }

    template<class Result>
    infra::Optional<Result> JsonObjectNavigator::operator/(JsonTransformOptionalObjectNavigatorToken<Result> token) const
    {
        auto subObject = object.GetOptionalObject(token.name);
        if (subObject == infra::none)
            return{};

        return infra::MakeOptional(token.transformation(*subObject));
    }

    template<class Result>
    Result JsonObjectNavigator::operator/(JsonTransformStringNavigatorToken<Result> token) const
    {
        auto member = object.GetOptionalString(token.name);
        if (member == infra::none)
            throw std::runtime_error(("String " + token.name + " not found").c_str());

        return token.transformation(member->ToStdString());
    }

    template<class Result>
    infra::Optional<Result> JsonObjectNavigator::operator/(JsonTransformOptionalStringNavigatorToken<Result> token) const
    {
        auto member = object.GetOptionalString(token.name);
        if (member == infra::none)
            return{};

        return infra::MakeOptional(token.transformation(member->ToStdString()));
    }

    template<class Result>
    infra::Optional<Result> JsonOptionalObjectNavigator::operator/(JsonTransformObjectNavigatorToken<Result> token) const
    {
        if (navigator != infra::none)
            return infra::MakeOptional(*navigator / token);
        else
            return{};
    }

    template<class Result>
    infra::Optional<Result> JsonOptionalObjectNavigator::operator/(JsonTransformOptionalObjectNavigatorToken<Result> token) const
    {
        if (navigator != infra::none)
            return infra::MakeOptional(*navigator / token);
        else
            return{};
    }

    template<class Result>
    infra::Optional<Result> JsonOptionalObjectNavigator::operator/(JsonTransformStringNavigatorToken<Result> token) const
    {
        if (navigator != infra::none)
            return infra::MakeOptional(*navigator / token);
        else
            return{};
    }

    template<class Result>
    infra::Optional<Result> JsonOptionalObjectNavigator::operator/(JsonTransformOptionalStringNavigatorToken<Result> token) const
    {
        if (navigator != infra::none)
            return infra::MakeOptional(*navigator / token);
        else
            return{};
    }
}

#endif