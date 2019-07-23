#pragma once

#include <iostream>

#include "botcraft/Protocol/BaseMessage.hpp"
#include "botcraft/Game/Enums.hpp"

#include "botcraft/Version.hpp"

namespace Botcraft
{
    class Respawn : public BaseMessage<Respawn>
    {
    public:
        virtual const int GetId() const override
        {
#if PROTOCOL_VERSION == 340 // 1.12.2
            return 0x35;
#elif PROTOCOL_VERSION == 393 || PROTOCOL_VERSION == 401 || PROTOCOL_VERSION == 404 // 1.13.X
            return 0x38;
#elif PROTOCOL_VERSION == 477 || PROTOCOL_VERSION == 480 || PROTOCOL_VERSION == 485 || PROTOCOL_VERSION == 490 || PROTOCOL_VERSION == 498 // 1.14.X
            return 0x3A;
#else
            #error "Protocol version not implemented"
#endif
        }

        virtual const std::string GetName() const override
        {
            return "Respawn";
        }

        const Dimension GetDimension() const
        {
            return dimension;
        }

#if PROTOCOL_VERSION < 477
        const Difficulty GetDifficulty() const
        {
            return difficulty;
        }
#endif

        const GameMode GetGamemode() const
        {
            return gamemode;
        }

        const std::string& GetLevelType() const
        {
            return level_type;
        }

    protected:
        virtual void ReadImpl(ReadIterator &iter, size_t &length) override
        {
            dimension = (Dimension)ReadData<int>(iter, length);
#if PROTOCOL_VERSION < 477
            difficulty = (Difficulty)ReadData<unsigned char>(iter, length);
#endif
            gamemode = (GameMode)ReadData<unsigned char>(iter, length);
            level_type = ReadString(iter, length);
        }

        virtual void WriteImpl(WriteContainer &container) const override
        {
            std::cerr << "Clientbound message" << std::endl;
        }

    private:
        Dimension dimension;
#if PROTOCOL_VERSION < 477
        Difficulty difficulty;
#endif
        GameMode gamemode;
        std::string level_type;
    };
} //Botcraft