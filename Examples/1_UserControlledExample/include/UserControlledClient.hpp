#pragma once

#include "botcraft/Game/InterfaceClient.hpp"

class UserControlledClient : public Botcraft::InterfaceClient
{
public:
    UserControlledClient(bool online,
        const std::vector<int> &printed_packets_ = std::vector<int>(0));
    ~UserControlledClient();
    
protected:

#ifdef USE_GUI
    void MouseCallback(const double &xoffset, const double &yoffset);
    void KeyBoardCallback(const std::array<bool, (int)Botcraft::Renderer::KEY_CODE::NUMBER_OF_KEYS> &is_key_pressed, const double &delta_time);
#endif    
    virtual void Handle(Botcraft::LoginSuccess &msg) override;

    void CreateTestWorld();

protected:

#if USE_GUI
    float mouse_sensitivity;
#endif
};