#include <functional>

#include "botcraft/Game/AssetsManager.hpp"
#include "botcraft/Game/World/Chunk.hpp"
#include "botcraft/Game/World/World.hpp"
#include "botcraft/Game/World/Block.hpp"
#include "botcraft/Game/World/Biome.hpp"
#include "botcraft/Game/Inventory/InventoryManager.hpp"
#include "botcraft/Game/Inventory/Inventory.hpp"
#include "botcraft/Game/BaseClient.hpp"

#include "botcraft/Network/NetworkManager.hpp"
#if USE_GUI
#include "botcraft/Renderer/RenderingManager.hpp"
#endif

using namespace ProtocolCraft;

namespace Botcraft
{
    BaseClient::BaseClient(const bool use_renderer_, const bool afk_only_)
    {
        afk_only = afk_only_;
        game_mode = GameMode::None;
        difficulty = Difficulty::None;
        is_hardcore = false;
#if PROTOCOL_VERSION > 463
        difficulty_locked = true;
#endif

        player = nullptr;
        world = nullptr;
        inventory_manager = nullptr;

#if USE_GUI
        use_renderer = use_renderer_;
        rendering_manager = nullptr;
#else
        if (use_renderer_)
        {
            std::cerr << "Warning, your version of botcraft hasn't been"
                << " compiled with GUI enabled, setting use_renderer_ to false" << std::endl;
        }
#endif
        auto_respawn = false;

        should_be_closed = false;

        // Ensure the assets are loaded
        AssetsManager::getInstance();
    }

    BaseClient::~BaseClient()
    {
        Disconnect();
    }

    void BaseClient::Connect(const std::string &ip, const unsigned int port, const std::string &login, const std::string &password)
    {
        network_manager = std::shared_ptr<NetworkManager>(new NetworkManager(ip, port, login, password));
        network_manager->AddHandler(this);
    }

    void BaseClient::RunSyncPos()
    {
        // Wait for 500 milliseconds before starting to send position continuously
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto last_send = std::chrono::system_clock::now();
        std::shared_ptr<PlayerPositionAndLookServerbound> msg_position(new PlayerPositionAndLookServerbound);
        bool has_moved = false;

        while (network_manager && network_manager->GetConnectionState() == ProtocolCraft::ConnectionState::Play)
        {
            // End of the current tick
            auto end = std::chrono::system_clock::now() + std::chrono::milliseconds(50);

            if (player && player->GetPosition().y < 1000.0)
            {
                if (afk_only)
                {
                    has_moved = false;
                }
                else
                {
                    std::lock_guard<std::mutex> player_guard(player->GetMutex());
                    //Check that we did not go through a block
                    Physics();

                    if (std::abs(player->GetSpeed().x) > 1e-3 ||
                        std::abs(player->GetSpeed().y) > 1e-3 ||
                        std::abs(player->GetSpeed().z) > 1e-3)
                    {
                        has_moved = true;
                    }
                    else
                    {
                        has_moved = false;
                    }

                    //Avoid forever falling if position is <= 0.0
                    if (player->GetPosition().y <= 0.0)
                    {
                        player->SetY(0.0);
                        player->SetSpeedY(0.0);
                        player->SetOnGround(true);
                    }

                    // Reset the speed until next frame
                    // Update the gravity value if needed
                    player->SetSpeedX(0.0);
                    player->SetSpeedZ(0.0);
                    if (player->GetOnGround())
                    {
                        player->SetSpeedY(0.0);
                    }
                    else
                    {
                        player->SetSpeedY((player->GetSpeed().y - 0.08) * 0.98);//TODO replace hardcoded value?
                    }
                }

#if USE_GUI
                if (use_renderer && has_moved)
                {
                    rendering_manager->SetPosOrientation(player->GetPosition().x, player->GetPosition().y + 1.62, player->GetPosition().z, player->GetYaw(), player->GetPitch());
                }
#endif
                msg_position->SetX(player->GetPosition().x);
                msg_position->SetFeetY(player->GetPosition().y);
                msg_position->SetZ(player->GetPosition().z);
                msg_position->SetYaw(player->GetYaw());
                msg_position->SetPitch(player->GetPitch());
                msg_position->SetOnGround(player->GetOnGround());

                if (network_manager && 
                    (has_moved || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_send).count() >= 1000))
                {
                    network_manager->Send(msg_position);
                    last_send = std::chrono::system_clock::now();
                }
            }
            std::this_thread::sleep_until(end);
        }
    }

    void BaseClient::Physics()
    {
        //If the player did not move we assume it does not collide
        if (abs(player->GetSpeed().x) < 1e-3 && 
            abs(player->GetSpeed().y) < 1e-3 && 
            abs(player->GetSpeed().z) < 1e-3)
        {
            return;
        }

        Vector3<double> min_player_collider, max_player_collider;
        
        for (int i = 0; i < 3; ++i)
        {
            min_player_collider[i] = std::min(player->GetCollider().GetMin()[i], player->GetCollider().GetMin()[i] + player->GetSpeed()[i]);
            max_player_collider[i] = std::max(player->GetCollider().GetMax()[i], player->GetCollider().GetMax()[i] + player->GetSpeed()[i]);
        }

        AABB broadphase_collider = AABB((min_player_collider + max_player_collider) / 2.0, (max_player_collider - min_player_collider) / 2.0);

        bool has_hit_down = false;
        bool has_hit_up = false;
        
        Position cube_pos;
        for (int x = (int)std::floor(min_player_collider.x); x < (int)std::ceil(max_player_collider.x); ++x)
        {
            cube_pos.x = x;
            for (int y = (int)std::floor(min_player_collider.y); y < (int)std::ceil(max_player_collider.y); ++y)
            {
                cube_pos.y = y;
                for (int z = (int)std::floor(min_player_collider.z); z < (int)std::ceil(max_player_collider.z); ++z)
                {
                    cube_pos.z = z;

                    Block block;
                    {
                        std::lock_guard<std::mutex> mutex_guard(world->GetMutex());
                        const Block *block_ptr = world->GetBlock(cube_pos);

                        if (block_ptr == nullptr)
                        {
                            continue;
                        }
                        block = *block_ptr;
                    }

                    if (!block.GetBlockstate()->IsSolid())
                    {
                        continue;
                    }

                    const std::vector<AABB> &block_colliders = block.GetBlockstate()->GetModel(block.GetModelId()).GetColliders();

                    for (int i = 0; i < block_colliders.size(); ++i)
                    {
                        if (!broadphase_collider.Collide(block_colliders[i] + Vector3<double>(cube_pos.x, cube_pos.y, cube_pos.z)))
                        {
                            continue;
                        }

                        Vector3<double> normal;
                        const double speed_fraction = player->GetCollider().SweptCollide(player->GetSpeed(), block_colliders[i] + Vector3<double>(cube_pos.x, cube_pos.y, cube_pos.z), normal);

                        if (speed_fraction < 1.0)
                        {
                            const Vector3<double> remaining_speed = player->GetSpeed() * (1.0 - speed_fraction);

                            // We remove epsilon to be sure we do not go
                            // through the face due to numerical imprecision
                            player->SetSpeed(player->GetSpeed() * (speed_fraction - 1e-6) + // Base speed truncated
                                (remaining_speed - normal * remaining_speed.dot(normal))); // Remaining speed projected on the plane
                        }

                        if (normal.y == 1.0)
                        {
                            has_hit_down = true;
                        }
                        else if (normal.y == -1.0)
                        {
                            has_hit_up = true;
                        }

                    }
                }
            }
        }
        player->SetPosition(player->GetPosition() + player->GetSpeed());
        player->SetOnGround(has_hit_down);
        if (has_hit_up)
        {
            player->SetSpeedY(0.0);
        }
    }

    void BaseClient::Disconnect()
    {
        game_mode = GameMode::None;
        difficulty = Difficulty::None;
#if PROTOCOL_VERSION > 463
        difficulty_locked = true;
#endif
        is_hardcore = false;

        network_manager.reset();
#if USE_GUI
        if (rendering_manager)
        {
            rendering_manager->Close();
        }
        rendering_manager.reset();
#endif

        if (m_thread_physics.joinable())
        {
            m_thread_physics.join();
        }

        if (world && !world->IsShared())
        {
            world.reset();
        }
        inventory_manager.reset();
        player.reset();
    }

    void BaseClient::SetSharedWorld(const std::shared_ptr<World> world_)
    {
        world = world_;
    }

    void BaseClient::Handle(Message &msg)
    {

    }

    void BaseClient::Handle(DisconnectLogin &msg)
    {
        std::cout << "Disconnect during login with reason: " << 
            msg.GetReason().GetText() << std::endl;
        std::cout << "Disconnecting ..." << std::endl;

        should_be_closed = true;
    }

    void BaseClient::Handle(LoginSuccess &msg)
    {
        if (!player)
        {
            player = std::shared_ptr<Player>(new Player);
        }
        if (!world)
        {
            world = std::shared_ptr<World>(new World(false));
            if (!afk_only)
            {
                network_manager->AddHandler(world.get());
            }
        }
        if (!inventory_manager)
        {
            inventory_manager = std::shared_ptr<InventoryManager>(new InventoryManager);
            if (!afk_only)
            {
                network_manager->AddHandler(inventory_manager.get());
            }
        }

#if USE_GUI
        if (use_renderer)
        {
            rendering_manager = std::shared_ptr<Renderer::RenderingManager>(new Renderer::RenderingManager(world, inventory_manager, 800, 600, AssetsManager::getInstance().GetTexturesPathsNames(), CHUNK_WIDTH, false));
            if (!afk_only)
            {
                network_manager->AddHandler(rendering_manager.get());
            }
        }
#endif
        
        // Launch the physics thread (continuously sending the position to the server)
        m_thread_physics = std::thread(&BaseClient::RunSyncPos, this);
    }

    void BaseClient::Handle(ServerDifficulty &msg)
    {
        difficulty = (Difficulty)msg.GetDifficulty();
#if PROTOCOL_VERSION > 463
        difficulty_locked = msg.GetDifficultyLocked();
#endif
    }

    void BaseClient::Handle(ConfirmTransactionClientbound &msg)
    {
        //If the transaction was not accepted, we must apologize
        if (!msg.GetAccepted())
        {
            std::shared_ptr<ConfirmTransactionServerbound> apologize_msg(new ConfirmTransactionServerbound);
            apologize_msg->SetWindowId(msg.GetWindowId());
            apologize_msg->SetActionNumber(msg.GetActionNumber());
            apologize_msg->SetAccepted(msg.GetAccepted());

            network_manager->Send(apologize_msg);
        }
        else
        {
            // TODO if accepted
        }
    }

    void BaseClient::Handle(DisconnectPlay &msg)
    {
        std::cout << "Disconnect during playing with reason: " << 
            msg.GetReason().GetText() << std::endl;
        std::cout << "Disconnecting ..." << std::endl;
        
        should_be_closed = true;
    }

    void BaseClient::Handle(JoinGame &msg)
    {
        player->GetMutex().lock();
        player->SetEID(msg.GetEntityId());
        player->GetMutex().unlock();
        game_mode = (GameMode)(msg.GetGamemode() & 0x03);
#if PROTOCOL_VERSION > 737
        is_hardcore = msg.GetIsHardcore();
#else
        is_hardcore = msg.GetGamemode() & 0x08;
#endif

#if PROTOCOL_VERSION < 464
        difficulty = (Difficulty)msg.GetDifficulty();
#endif
    }

    void BaseClient::Handle(Entity &msg)
    {
        std::cout << "New entity created with ID: " << msg.GetEntityId() << std::endl;
        //TODO add the entity to the world
    }

    void BaseClient::Handle(EntityRelativeMove &msg)
    {
        std::lock_guard<std::mutex> player_guard(player->GetMutex());
        if (msg.GetEntityId() == player->GetEID())
        {
            player->SetX((msg.GetDeltaX() / 128.0f + player->GetPosition().x * 32.0f) / 32.0f);
            player->SetY((msg.GetDeltaY() / 128.0f + player->GetPosition().y * 32.0f) / 32.0f);
            player->SetZ((msg.GetDeltaZ() / 128.0f + player->GetPosition().z * 32.0f) / 32.0f);
            player->SetOnGround(msg.GetOnGround());

#ifdef USE_GUI
            if (use_renderer)
            {
                rendering_manager->SetPosOrientation(player->GetPosition().x, player->GetPosition().y + 1.62f, player->GetPosition().z, player->GetYaw(), player->GetPitch());
            }
#endif // USE_GUI
        }
        else
        {
            //TODO Change entity's position in the world
        }
    }

    void BaseClient::Handle(EntityLookAndRelativeMove &msg)
    {
        std::lock_guard<std::mutex> player_guard(player->GetMutex());
        if (msg.GetEntityId() == player->GetEID())
        {
            player->SetX((msg.GetDeltaX() / 128.0f + player->GetPosition().x * 32.0f) / 32.0f);
            player->SetY((msg.GetDeltaY() / 128.0f + player->GetPosition().y * 32.0f) / 32.0f);
            player->SetZ((msg.GetDeltaZ() / 128.0f + player->GetPosition().z * 32.0f) / 32.0f);
            player->SetYaw(360.0f * msg.GetYaw() / 256.0f);
            player->SetPitch(360.0f * msg.GetPitch() / 256.0f);
            player->SetOnGround(msg.GetOnGround());

#ifdef USE_GUI
            if (use_renderer)
            {
                rendering_manager->SetPosOrientation(player->GetPosition().x, player->GetPosition().y + 1.62f, player->GetPosition().z, player->GetYaw(), player->GetPitch());
            }
#endif // USE_GUI
        }
        else
        {
            //TODO Change entity's position and rotation in the world
        }
    }

    void BaseClient::Handle(EntityLook &msg)
    {
        std::lock_guard<std::mutex> player_guard(player->GetMutex());
        if (msg.GetEntityId() == player->GetEID())
        {
            player->SetYaw(360.0f * msg.GetYaw() / 256.0f);
            player->SetPitch(360.0f * msg.GetPitch() / 256.0f);
            player->SetOnGround(msg.GetOnGround());

#ifdef USE_GUI
            if (use_renderer)
            {
                rendering_manager->SetPosOrientation(player->GetPosition().x, player->GetPosition().y + 1.62f, player->GetPosition().z, player->GetYaw(), player->GetPitch());
            }
#endif // USE_GUI
        }
        else
        {
            //TODO Change entity's rotation in the world
        }
    }

    void BaseClient::Handle(PlayerPositionAndLookClientbound &msg)
    {
        std::lock_guard<std::mutex> player_guard(player->GetMutex());
        (msg.GetFlags() & 0x01) ? player->SetX(player->GetPosition().x + msg.GetX()) : player->SetX(msg.GetX());
        (msg.GetFlags() & 0x02) ? player->SetY(player->GetPosition().y + msg.GetY()) : player->SetY(msg.GetY());
        (msg.GetFlags() & 0x04) ? player->SetZ(player->GetPosition().z + msg.GetZ()) : player->SetZ(msg.GetZ());
        (msg.GetFlags() & 0x08) ? player->SetYaw(player->GetYaw() + msg.GetYaw()) : player->SetYaw(msg.GetYaw());
        (msg.GetFlags() & 0x10) ? player->SetPitch(player->GetPitch() + msg.GetPitch()) : player->SetPitch(msg.GetPitch());

        std::shared_ptr<TeleportConfirm> confirm_msg(new TeleportConfirm);
        confirm_msg->SetTeleportId(msg.GetTeleportId());

        network_manager->Send(confirm_msg);

#ifdef USE_GUI
        if (use_renderer)
        {
            rendering_manager->SetPosOrientation(player->GetPosition().x, player->GetPosition().y + 1.62f, player->GetPosition().z, player->GetYaw(), player->GetPitch());
        }
#endif // USE_GUI
    }

    void BaseClient::Handle(UpdateHealth &msg)
    {
        {
            std::lock_guard<std::mutex> player_lock(player->GetMutex());
            player->SetHealth(msg.GetHealth());
            player->SetFood(msg.GetFood());
            player->SetFoodSaturation(msg.GetFoodSaturation());
        }

        if (msg.GetHealth() <= 0.0f && auto_respawn)
        {
            std::shared_ptr<ClientStatus> status_message(new ClientStatus);
            status_message->SetActionID(0);
            network_manager->Send(status_message);
        }
    }

    void BaseClient::Handle(EntityTeleport &msg)
    {
        std::lock_guard<std::mutex> player_guard(player->GetMutex());
        if (msg.GetEntityId() == player->GetEID())
        {
            player->SetX(msg.GetX());
            player->SetY(msg.GetY());
            player->SetZ(msg.GetZ());
            player->SetYaw(360.0f * msg.GetYaw() / 256.0f);
            player->SetPitch(360.0f * msg.GetPitch() / 256.0f);
            player->SetOnGround(msg.GetOnGround());

#ifdef USE_GUI
            if (use_renderer)
            {
                rendering_manager->SetPosOrientation(player->GetPosition().x, player->GetPosition().y + 1.62f, player->GetPosition().z, player->GetYaw(), player->GetPitch());
            }
#endif // USE_GUI
        }
        else
        {
            //TODO Change entity's position and rotation in the world
        }
    }

    void BaseClient::Handle(PlayerAbilitiesClientbound &msg)
    {
        {
            std::lock_guard<std::mutex> player_guard(player->GetMutex());
            player->SetIsInvulnerable(msg.GetFlags() & 0x01);
            player->SetIsFlying(msg.GetFlags() & 0x02);
            player->SetFlyingSpeed(msg.GetFlyingSpeed());
        }
        allow_flying = msg.GetFlags() & 0x04;
        creative_mode = msg.GetFlags() & 0x08;

        //TODO do something with the field of view modifier


        std::shared_ptr<ClientSettings> settings_msg(new ClientSettings);
        settings_msg->SetLocale("fr_FR");
        settings_msg->SetViewDistance(10);
        settings_msg->SetChatMode((int)ChatMode::Enabled);
        settings_msg->SetChatColors(true);
        settings_msg->SetDisplayedSkinParts(0xFF);
        settings_msg->SetMainHand((int)Hand::Right);

        network_manager->Send(settings_msg);
    }

    void BaseClient::Handle(Respawn &msg)
    {
#if PROTOCOL_VERSION < 464
        difficulty = (Difficulty)msg.GetDifficulty();
#endif
        game_mode = (GameMode)msg.GetGamemode();
    }
} //Botcraft