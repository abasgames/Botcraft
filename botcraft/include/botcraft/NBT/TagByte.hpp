#pragma once

#include <memory>

#include "botcraft/NBT/Tag.hpp"

namespace Botcraft
{
    class TagByte : public Tag
    {
    public:
        TagByte();
        ~TagByte();

        const char GetValue() const;
        void SetValue(const char v);

        virtual const TagType GetType() const override;

        virtual void Read(ReadIterator &iterator, size_t &length) override;
        virtual void Write(WriteContainer &container) const override;
        virtual const std::string Print(const std::string &prefix) const override;

    private:
        char value;
    };
}