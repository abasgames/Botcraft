#pragma once

#include "protocolCraft/BaseMessage.hpp"

namespace ProtocolCraft
{
    class PlayerPositionAndLookClientbound : public BaseMessage<PlayerPositionAndLookClientbound>
    {
    public:
        virtual const int GetId() const override
        {
#if PROTOCOL_VERSION == 340 // 1.12.2
            return 0x2F;
#elif PROTOCOL_VERSION == 393 || PROTOCOL_VERSION == 401 || PROTOCOL_VERSION == 404 // 1.13.X
            return 0x32;
#elif PROTOCOL_VERSION == 477 || PROTOCOL_VERSION == 480 || PROTOCOL_VERSION == 485 || PROTOCOL_VERSION == 490 || PROTOCOL_VERSION == 498 // 1.14.X
            return 0x35;
#elif PROTOCOL_VERSION == 573 || PROTOCOL_VERSION == 575 || PROTOCOL_VERSION == 578 // 1.15.X
			return 0x36;
#elif PROTOCOL_VERSION == 735 || PROTOCOL_VERSION == 736  // 1.16.0 or 1.16.1
            return 0x35;
#elif PROTOCOL_VERSION == 751 || PROTOCOL_VERSION == 753 // 1.16.2, 1.16.3
            return 0x34;
#else
            #error "Protocol version not implemented"
#endif
        }

        virtual const std::string GetName() const override
        {
            return "Player Position And Look (clientbound)";
        }

        void SetX(const double x_)
        {
            x = x_;
        }

        void SetY(const double y_)
        {
            y = y_;
        }

        void SetZ(const double z_)
        {
            z = z_;
        }

        void SetYaw(const float yaw_)
        {
            yaw = yaw_;
        }

        void SetPitch(const float pitch_)
        {
            pitch = pitch_;
        }

        void SetFlags(const char flags_)
        {
            flags = flags_;
        }

        void SetTeleportId(const int teleport_id_)
        {
            teleport_id = teleport_id_;
        }

        const double GetX() const
        {
            return x;
        }

        const double GetY() const
        {
            return y;
        }

        const double GetZ() const
        {
            return z;
        }

        const float GetYaw() const
        {
            return yaw;
        }

        const float GetPitch() const
        {
            return pitch;
        }

        const char GetFlags() const
        {
            return flags;
        }

        const int GetTeleportId() const
        {
            return teleport_id;
        }

    protected:
        virtual void ReadImpl(ReadIterator &iter, size_t &length) override
        {
            x = ReadData<double>(iter, length);
            y = ReadData<double>(iter, length);
            z = ReadData<double>(iter, length);
            yaw = ReadData<float>(iter, length);
            pitch = ReadData<float>(iter, length);
            flags = ReadData<char>(iter, length);
            teleport_id = ReadVarInt(iter, length);
        }

        virtual void WriteImpl(WriteContainer &container) const override
        {
            WriteData<double>(x, container);
            WriteData<double>(y, container);
            WriteData<double>(z, container);
            WriteData<float>(yaw, container);
            WriteData<float>(pitch, container);
            WriteData<char>(flags, container);
            WriteVarInt(teleport_id, container);
        }

        virtual const picojson::value SerializeImpl() const override
        {
            picojson::value value(picojson::object_type, false);
            picojson::object& object = value.get<picojson::object>();

            object["x"] = picojson::value(x);
            object["y"] = picojson::value(y);
            object["z"] = picojson::value(z);
            object["yaw"] = picojson::value((double)yaw);
            object["pitch"] = picojson::value((double)pitch);
            object["flags"] = picojson::value((double)flags);
            object["teleport_id"] = picojson::value((double)teleport_id);

            return value;
        }

    private:
        double x;
        double y;
        double z;
        float yaw;
        float pitch;
        char flags;
        int teleport_id;
    };
}