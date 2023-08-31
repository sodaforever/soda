#pragma once

// mysql util

#include <mysql/mysql.h>
#include "../general/util.hpp"

namespace soda
{

    enum_field_types get_field_type(MYSQL_FIELD *field)
    {
        return field->type;
    }

    const char *get_field_name(MYSQL_FIELD *field)
    {
        return field->name;
    }

    size_t get_field_size(MYSQL_FIELD *field)
    {
        switch (field->type)
        {
        case MYSQL_TYPE_TINY:
            return 1;
        case MYSQL_TYPE_SHORT:
            return 2;
        case MYSQL_TYPE_INT24:
            return 3;
        case MYSQL_TYPE_LONG:
            return 4;
        case MYSQL_TYPE_LONGLONG:
            return 8;
        case MYSQL_TYPE_FLOAT:
            return 4;
        case MYSQL_TYPE_DOUBLE:
            return 8;
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
            return sizeof(MYSQL_TIME);
        case MYSQL_TYPE_YEAR:
            return 1;
        case MYSQL_TYPE_NEWDATE:
            return 3;
        case MYSQL_TYPE_ENUM:
            return 2;
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return field->max_length + 3;
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_JSON:
            return field->max_length + 1;
        case MYSQL_TYPE_BIT:
        case MYSQL_TYPE_TIMESTAMP2:
        case MYSQL_TYPE_DATETIME2:
        case MYSQL_TYPE_TIME2:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_GEOMETRY:
        default:
            return field->max_length;
        }
    }

    template <typename T, typename Enable = void>
    struct MySQLTypeInfo
    {
    };

    template <typename T, enum_field_types Type, bool IsUnsigned = false>
    struct MySQLTypeInfoBase
    {
        static const enum_field_types value = Type;
        static const bool is_unsigned = IsUnsigned;
    };

    // Specialization for pointers and nullptr
    template <typename T>
    struct MySQLTypeInfo<T, enable_if_t<std::is_null_pointer<T>::value>> : public MySQLTypeInfoBase<T, MYSQL_TYPE_NULL>
    {
    };

    // Specializations for integral types
    template <>
    struct MySQLTypeInfo<int8_t> : public MySQLTypeInfoBase<int8_t, MYSQL_TYPE_TINY, false>
    {
    };

    template <>
    struct MySQLTypeInfo<uint8_t> : public MySQLTypeInfoBase<uint8_t, MYSQL_TYPE_TINY, true>
    {
    };

    template <>
    struct MySQLTypeInfo<int16_t> : public MySQLTypeInfoBase<int16_t, MYSQL_TYPE_SHORT, false>
    {
    };

    template <>
    struct MySQLTypeInfo<uint16_t> : public MySQLTypeInfoBase<uint16_t, MYSQL_TYPE_SHORT, true>
    {
    };

    template <>
    struct MySQLTypeInfo<int32_t> : public MySQLTypeInfoBase<int32_t, MYSQL_TYPE_LONG, false>
    {
    };

    template <>
    struct MySQLTypeInfo<uint32_t> : public MySQLTypeInfoBase<uint32_t, MYSQL_TYPE_LONG, true>
    {
    };

    template <>
    struct MySQLTypeInfo<int64_t> : public MySQLTypeInfoBase<int64_t, MYSQL_TYPE_LONGLONG, false>
    {
    };

    template <>
    struct MySQLTypeInfo<uint64_t> : public MySQLTypeInfoBase<uint64_t, MYSQL_TYPE_LONGLONG, true>
    {
    };

    // Specializations for floating point types
    template <>
    struct MySQLTypeInfo<float> : public MySQLTypeInfoBase<float, MYSQL_TYPE_FLOAT>
    {
    };

    template <>
    struct MySQLTypeInfo<double> : public MySQLTypeInfoBase<double, MYSQL_TYPE_DOUBLE>
    {
    };

    // Specialization for std::string
    template <>
    struct MySQLTypeInfo<std::string> : public MySQLTypeInfoBase<std::string, MYSQL_TYPE_STRING>
    {
    };

    // For BIT fields
    template <>
    struct MySQLTypeInfo<bool> : public MySQLTypeInfoBase<bool, MYSQL_TYPE_BIT>
    {
    };

    // For DECIMAL or NUMERIC fields (as string)
    template <>
    struct MySQLTypeInfo<char *> : public MySQLTypeInfoBase<char *, MYSQL_TYPE_STRING>
    {
    };

    // For BLOB fields
    template <>
    struct MySQLTypeInfo<uint8_t *> : public MySQLTypeInfoBase<uint8_t *, MYSQL_TYPE_BLOB>
    {
    };

    // Specializations for MySQL time structures
    template <>
    struct MySQLTypeInfo<MYSQL_TIME> : public MySQLTypeInfoBase<MYSQL_TIME, MYSQL_TYPE_DATETIME>
    {
    };

    template <typename T>
    struct MySQLTypeInfoUni
    {
        static const decltype(MySQLTypeInfo<rm_cvref_t<T>>::value) value = MySQLTypeInfo<rm_cvref_t<T>>::value;
        static const bool is_unsigned = MySQLTypeInfo<rm_cvref_t<T>>::is_unsigned;
    };

} // namespace soda
