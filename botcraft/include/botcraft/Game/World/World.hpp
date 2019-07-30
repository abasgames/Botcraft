#pragma once

#include <map>
#include <array>
#include <memory>

#include "botcraft/Game/Vector3.hpp"
#include "botcraft/Game/Enums.hpp"
#include "botcraft/Version.hpp"
#include "botcraft/Game/OtherPlayer.hpp"

namespace Botcraft
{
    class Chunk;
    class Block;
    class NBT;
    class Blockstate;

    class World
    {
    public:
        World();

        bool AddChunk(const int x, const int z, const Dimension dim);
        bool RemoveChunk(const int x, const int z);

#if PROTOCOL_VERSION < 347
        bool SetBlock(const Position &pos, const unsigned int id, unsigned char metadata, const int model_id = -1);
#else
        bool SetBlock(const Position &pos, const unsigned int id, const int model_id = -1);
#endif

        bool SetBlockEntityData(const Position &pos, const std::shared_ptr<NBT> data);

#if PROTOCOL_VERSION < 358
        bool SetBiome(const int x, const int z, const unsigned char biome);
#else
        bool SetBiome(const int x, const int z, const int biome);
#endif
        bool SetSkyLight(const Position &pos, const unsigned char skylight);
        bool SetBlockLight(const Position &pos, const unsigned char blocklight);

        //Get a pointer on a chunk (or null if not loaded)
        std::shared_ptr<Chunk> GetChunk(const int x, const int z);

        //Get the block at a given position
        const Block *GetBlock(const Position &pos);
        // Get the block entity data at a given position
        std::shared_ptr<NBT> GetBlockEntityData(const Position &pos);

#if PROTOCOL_VERSION < 358
        const unsigned char GetBiome(const Position &pos);
#else
        const int GetBiome(const Position &pos);
#endif
        const unsigned char GetSkyLight(const Position &pos);
        const unsigned char GetBlockLight(const Position &pos);
        const Dimension GetDimension(const int x, const int z);

        /**
        * Perform a raycast in the voxel world and return position, normal and blockstate which are hit
        *
        * @param[in] origin the origin of the ray
        * @param[in] direction the direction of the ray
        * @param[in] max_radius maximum distance of the search, must be > 0
        * @param[out] out_pos the position of the block hit
        * @param[out] out_normal the normal of the face hit
        * @return the blockstate of the hit cube (or null)
        */
        std::shared_ptr<Blockstate> Raycast(const Vector3<double> &origin, const Vector3<double> &direction,
            const float max_radius, Position &out_pos, Position &out_normal);

        // Get the list of chunks
        const std::map<std::pair<int, int>, std::shared_ptr<Chunk> >& GetAllChunks() const;

        void AddPlayer(const OtherPlayer &p);
        std::map<std::string, OtherPlayer>::iterator GetPlayer(const std::string &uuid);
        const std::map<std::string, OtherPlayer>& GetAllPlayers() const;
        void RemovePlayer(const std::string &uuid);

    private:
        int cached_x;
        int cached_z;
        std::shared_ptr<Chunk> cached;

        std::map<std::pair<int, int>, std::shared_ptr<Chunk> > terrain;

        std::map<std::string, OtherPlayer> all_players;
    };
} // Botcraft