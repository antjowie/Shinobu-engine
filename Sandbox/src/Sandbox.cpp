#include "Sandbox.h"
#include "Component.h"
#include "System.h"



std::unordered_map<sh::Entity, sh::Entity> ClientLayer::m_netToLocal;

void DrawSceneStats(sh::Scene& scene)
{
    auto& reg = scene.GetRegistry().Get();
    ImGui::Text("Simulation %i (%i/%i)", scene.GetSimulationCount(), scene.GetCurrentSimulationIndex(), scene.maxSimulations);
    ImGui::Text("Entities %i", reg.size());
}

std::unique_ptr<sh::Application> sh::CreateApplication()
{
    sh::Registry::RegisterComponent<Player>();
    sh::Registry::RegisterComponent<::Transform>();
    sh::Registry::RegisterComponent<Sprite>();
    sh::Registry::RegisterComponent<SpawnEntity>();

    auto app = std::make_unique<sh::Application>();
    app->GetLayerStack().PushLayer(new MainMenuLayer());

    return app;
}

void MainMenuLayer::OnEvent(sh::Event& event)
{
    sh::EventDispatcher d(event);
    d.Dispatch<sh::KeyPressedEvent>([&](sh::KeyPressedEvent& e)
        {
            if (e.GetKeyCode() == sh::KeyCode::Escape)
                sh::Application::Get().Exit();
            return false;
        });
}

void MainMenuLayer::OnGuiRender()
{
    if (!ImGui::Begin("Options"))
        return;

    // TODO: If server or client fails this doesn't get updated
    // it should be communicated via events
    static bool client = false;
    static char serverIP[32] = "127.0.0.1";
    if (ImGui::Checkbox("Client", &client))
    {
        if (client)
        {
            m_client = new ClientLayer();
            sh::Application::Get().GetLayerStack().PushLayer(m_client);
            sh::Application::Get().OnEvent(sh::ClientConnectRequestEvent(serverIP, 25565));
        }
        else
        {
            sh::Application::Get().GetLayerStack().PopLayer(m_client);
            delete m_client;
            m_client = nullptr;
        }
    }

    ImGui::SameLine(); ImGui::Text("ip ");
    ImGui::SameLine(); ImGui::InputText("#IP", serverIP, 32);

    static bool server = false;
    if (ImGui::Checkbox("Server", &server))
    {
        if (server)
        {
            m_server = new ServerLayer();
            sh::Application::Get().GetLayerStack().PushLayer(m_server);
        }
        else
        {
            sh::Application::Get().GetLayerStack().PopLayer(m_server);
            delete m_server;
            m_server = nullptr;
        }
    }

    ImGui::End();
}

sh::Entity ClientLayer::LocalIDToNet(sh::Entity localID) 
{
    for (const auto e : m_netToLocal)
        if (e.second == localID) return e.first;
    SH_CORE_ERROR("Local ID {} can't be mapped to a network ID", localID);
}

sh::Entity ClientLayer::NetIDtoLocal(sh::Entity netID)
{
    return m_netToLocal.at(netID);
}

void ClientLayer::OnAttach()
{
    m_scene.RegisterSystem(SpawnSystem);
    m_scene.RegisterSystem(InputSystem);
    m_scene.RegisterSystem(DrawSystem(m_camera.GetCamera()));
}

void ClientLayer::OnDetach()
{
    // TODO: add timeout variable to this so that we can just do 0 instead of 5
    sh::Application::Get().OnEvent(sh::ClientDisconnectRequestEvent());
    m_netToLocal.clear();
}

void ClientLayer::OnEvent(sh::Event& event)
{
    m_camera.OnEvent(event);

    sh::EventDispatcher d(event);

    if (d.Dispatch<sh::ClientReceivePacketEvent>([&](sh::ClientReceivePacketEvent& e)
        {
#if 0
            // Get the right entity and check if server entity exists in view
            auto& p = e.GetPacket();
            auto netID = sh::Entity(p.entity);

            auto match = m_netToLocal.find(netID);
            if (match == m_netToLocal.end()) { m_netToLocal[netID] = m_scene.GetRegistry().Create(); }

            sh::Entity local = m_netToLocal[netID];
            //SH_TRACE("Client received type ({}) for entity (local: {} net: {})", 
            //    m_reg.GetComponentData().at(p.id).name,
            //    local,netID);

            m_scene.GetRegistry().HandlePacket(local, p);
#endif
            //SH_TRACE(e.GetPacket().simulation);
            SH_TRACE("PUSHED {}", sh::Registry::GetComponentData().at(e.GetPacket().id).name);

            m_packets.Push(e.GetPacket(),true);
            return false;
        })) return;

    // Honestly, this may be quite a dumb way of doing it.
    // This is because the event gets modified along the way so it may get confusing where the event will pass through
    // and where not
    if (d.Dispatch<sh::ClientSendPacketEvent>([&](sh::ClientSendPacketEvent& e)
        {
            e.GetPacket().clientSimulation = m_scene.GetSimulationCount();

            return false;
        })) return;
}

void ClientLayer::OnUpdate(sh::Timestep ts)
{
    auto& client = sh::NetClient::Get();
    if (!client.IsConnected()) return;

    m_camera.OnUpdate(ts);
    
    // Poll packets
    sh::Packet p;
    SH_INFO("Polling packets");

    m_packets.Swap();
    while(m_packets.Poll(p))
    {
        SH_TRACE(sh::Registry::GetComponentData().at(p.id).name);
        // Get the right entity and check if server entity exists in current reg
        auto netID = sh::Entity(p.entity);

        auto match = m_netToLocal.find(netID);
        if (match == m_netToLocal.end()) { m_netToLocal[netID] = m_scene.GetRegistry().Create(); }

        sh::Entity local = m_netToLocal[netID];
        //SH_TRACE("Client received type ({}) for entity (local: {} net: {})", 
        //    m_reg.GetComponentData().at(p.id).name,
        //    local,netID);

        // This happens when a server sends a response
        // For example, player joining, chunk request
        if (p.clientSimulation == 0)
        {
            m_scene.GetRegistry().HandlePacket(local, p);
        }
        else if (m_scene.GetRegistry(m_scene.GetSimulationCount() - p.clientSimulation).HandlePacket(local, p))
        {
            SH_WARN("Server reconciliation took place!!!");
            // TODO: Reconciliate subsequent registries
            m_scene.GetRegistry().HandlePacket(local, p);
        }
    }

    m_scene.Simulate(ts);
}

void ClientLayer::OnGuiRender()
{
    if (ImGui::Begin("Stats"))
    {
        if(ImGui::CollapsingHeader("Client"))
        {
            DrawSceneStats(m_scene);
        }
        ImGui::End();
    }
}

void ServerLayer::OnEvent(sh::Event& event)
{
    m_camera.OnEvent(event);

    sh::EventDispatcher e(event);

    if (e.Dispatch<sh::ServerClientConnectEvent>([&](sh::ServerClientConnectEvent& e)
        {
            auto& reg = m_scene.GetRegistry();
            auto& app = sh::Application::Get();

            char ip[64];
            enet_address_get_host_ip(&e.GetPeer()->address, ip, 64);

            // TODO: Create a join component that is handled by the ECS
            // Create the new player
            auto entity = reg.Create();
            reg.Get().emplace<Transform>(entity, glm::vec2(0.f));
            auto& sprite = reg.Get().emplace<Sprite>(entity);
            sprite.image = "res/image.png";
            sprite.LoadTexture();

            // Submit the new player
            app.OnEvent(sh::ServerSendPacketEvent(sh::Serialize(Player(), entity), e.GetPeer()));

            // Send all existing users to that player
            auto view = reg.Get().view<Transform, Sprite>();
            for (auto ent : view)
            {
                auto& t = reg.Get().get<Transform>(ent);
                auto& s = reg.Get().get<Sprite>(ent);

                SpawnEntity spawn;
                spawn.type = SpawnEntity::Player;
                spawn.t = t;
                spawn.sprite = s;
                app.OnEvent(sh::ServerSendPacketEvent(sh::Serialize(spawn, ent), e.GetPeer()));
            }

            // Broadcast new player
            SpawnEntity ent;
            ent.type = SpawnEntity::Player;
            ent.t.pos = glm::vec2(0.f);
            ent.sprite.image = "res/image.png";

            app.OnEvent(sh::ServerBroadcastPacketEvent(
                sh::Serialize(ent, entity)));
            
            return true;
        })) return;

    if (e.Dispatch<sh::ServerClientConnectEvent>([&](sh::ServerClientConnectEvent& e)
        {
            char ip[64];
            enet_address_get_host_ip(&e.GetPeer()->address, ip, 64);
            //SH_INFO("Server closed connection with {}", ip);
            return true;
        })) return;

    if (e.Dispatch<sh::ServerSendPacketEvent>([&](sh::ServerSendPacketEvent& e)
        {
            e.GetPacket().serverSimulation = m_scene.GetSimulationCount();
            return false;
        })) return;
    if (e.Dispatch<sh::ServerBroadcastPacketEvent>([&](sh::ServerBroadcastPacketEvent& e)
        {
            e.GetPacket().serverSimulation = m_scene.GetSimulationCount();
            return false;
        })) return;

    if (e.Dispatch<sh::ServerReceivePacketEvent>([&](sh::ServerReceivePacketEvent& e)
        {
            // TODO: Handle the input
            m_packets.Push(e.GetPacket(),false);
            //m_scene.GetRegistry().HandlePacket(sh::Entity(e.GetPacket().entity), e.GetPacket());
            return true;
        })) return;

}

void ServerLayer::OnAttach()
{
    m_scene.RegisterSystem(SpawnSystem);
    //m_reg.RegisterSystem(DrawSystem(m_camera.GetCamera()));

    sh::Application::Get().OnEvent(sh::ServerHostRequestEvent(25565));
}

void ServerLayer::OnDetach()
{
    sh::Application::Get().OnEvent(sh::ServerShutdownRequestEvent());
}

void ServerLayer::OnUpdate(sh::Timestep ts)
{
    auto& server = sh::NetServer::Get();
    if (!server.IsHosting()) return;

    m_camera.OnUpdate(ts);
    
    sh::Packet p;
    m_packets.Swap();
    while (m_packets.Poll(p))
    {
        m_scene.GetRegistry().HandlePacket(sh::Entity(p.entity), p);
    }
    m_scene.Simulate(ts);

    // TODO: Should or could be handled in systems
    auto& reg = m_scene.GetRegistry().Get();
    auto view = reg.view<Transform>();
    for (auto e : view)
    {
        auto p = sh::Serialize(view.get(e), e);
        sh::Application::Get().OnEvent(sh::ServerBroadcastPacketEvent(p));

        //SH_TRACE("Server X:{}", t.pos.x);
    }
}

void ServerLayer::OnGuiRender()
{
    if (ImGui::Begin("Stats"))
    {
        if (ImGui::CollapsingHeader("Server"))
        {
            DrawSceneStats(m_scene);
        }
        ImGui::End();
    }
}