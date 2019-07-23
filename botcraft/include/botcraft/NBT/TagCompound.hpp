#pragma once

#include <memory>
#include <map>

#include "botcraft/NBT/Tag.hpp"

namespace Botcraft
{
    class TagCompound : public Tag
    {
    public:
        TagCompound();
        ~TagCompound();

        const std::map<std::string, std::shared_ptr<Tag> >& GetValues() const;
        void SetValues(const std::map<std::string, std::shared_ptr<Tag> > &v);

        virtual const TagType GetType() const override;

        virtual void Read(ReadIterator &iterator, size_t &length) override;
        virtual void Write(WriteContainer &container) const override;
        virtual const std::string Print(const std::string &prefix) const override;

    private:
        std::map<std::string, std::shared_ptr<Tag> > tags;
    };
}