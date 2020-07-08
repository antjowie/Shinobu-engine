#include <Shinobu/Common.h>
#include <Shinobu/Net/Client.h>
#include <Shinobu/Net/Server.h>

//#include <glm/gtc/matrix_transform.hpp>

class ExampleLayer2D : public sh::Layer
{
public:
    ExampleLayer2D() 
        : sh::Layer("Example Layer") 
        , m_camera(16.f/9.f){}

    std::shared_ptr<sh::Texture> tex;
    float radians = 0.f;
    float yPos = 0.f;
    sh::OrthographicCameraController m_camera;

    sh::Server m_server;
    sh::Client m_client;

    void ToggleServer()
    {
        if (m_server.IsHosting()) m_server.Shutdown();
        else m_server.Host(25565);
    }

    void ToggleClient()
    {

        if (m_client.IsConnected() || m_client.IsConnecting()) m_client.Disconnect();
        else m_client.Connect("localhost", 25565);
    }

    virtual void OnAttach() override 
    {
        // 1) Important: globals are not shared across DLL boundaries! If you use DLLs or any form of hot-reloading: you will need to call
        //    SetCurrentContext() (with the pointer you got from CreateContext) from each unique static/DLL boundary, and after each hot-reloading.
        //    In your debugger, add GImGui to your watch window and notice how its value changes depending on which location you are currently stepping into.
        //ImGui::SetCurrentContext(sh::ImGuiLayer::GetContext());

        tex = sh::Texture2D::Create("res/image.png");

        sh::ExampleData data;
        data.message = "Hello world lol I'm packet";
        auto packet = sh::Serialize(data);
        auto newData = sh::Deserialize<sh::ExampleData>(packet);
        SH_CORE_TRACE(newData.message);

        // using ConnectCB = std::function<void(Server&, ENetPeer* connection)>;
        m_server.SetConnectCB([](sh::Server& m_server, ENetPeer* connection)
            {
                char ip[64];
                enet_address_get_host_ip(&connection->address, ip, 64);
                SH_INFO("Server opened connection with {}", ip);
            });
        m_server.SetDisconnectCB([](sh::Server& m_server, ENetPeer* connection)
            {
                char ip[64];
                enet_address_get_host_ip(&connection->address, ip, 64);
                SH_INFO("Server closed connection with {}", ip);
            });
    }
    
    virtual void OnDetach() override 
    { 
        SH_INFO("Detached {0}", GetName()); 

        // If user runs client, we need to keep updating the server so that the server still sends disconnect packets
        if (m_client.IsConnected() && m_server.IsHosting())
        {
            auto future = m_client.Disconnect(); 
            while (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                sh::Packet p;
                m_server.Poll(p);
            }
        }
    }

    virtual void OnEvent(sh::Event& event) override 
    {
        m_camera.OnEvent(event);

        sh::EventDispatcher d(event); 
        d.Dispatch<sh::KeyPressedEvent>([&](sh::KeyPressedEvent& e)
        {
            if (e.GetKeyCode() == sh::KeyCode::Escape)
                sh::Application::Get().Exit();
            return false;
        });
    }

    virtual void OnUpdate(sh::Timestep ts) override final
    {
        m_camera.OnUpdate(ts);

        sh::Packet p;
        if (m_server.IsHosting()) while (m_server.Poll(p));
        if (m_client.IsConnected()) while (m_client.Poll(p));
        if (m_client.IsConnecting())
        {

        }

        sh::Renderer2D::BeginScene(m_camera.GetCamera());
        //// Draw a quad
        //sh::Renderer2D::Submit(sh::Render2DData(glm::vec2(0.f), glm::vec2(1.f), glm::vec4(1.f)));
        //// Draw a rotated quad
        //sh::Renderer2D::Submit(sh::Render2DData(glm::vec2(1.f,0.1f), glm::vec2(1.f), glm::vec4(0.5f,1.f,1.f,1.f), radians));
        //// Draw a textured quad
        //sh::Renderer2D::Submit(sh::Render2DData(glm::vec2(0.f,yPos), glm::vec2(3.f),tex));

        sh::Renderer2D::Submit(sh::Render2DData(glm::vec2(0.f), glm::vec2(1.f), tex, radians, glm::vec4(0.5f, 1.f, 1.f, 1.f)));

        sh::Renderer2D::EndScene();
    }

    virtual void OnGuiRender() override final
    {
        if (!ImGui::Begin("Renderer 2D"))
        {
            // Early out if the window is collapsed, as an optimization.
            ImGui::End();
            return;
        }
        ImGui::SliderAngle("rotation", &radians);
        ImGui::SliderFloat("yPos", &yPos,-10.f,10.f);
        ImGui::End();
        
        if (!ImGui::Begin("Network"))
        {
            // Early out if the window is collapsed, as an optimization.
            ImGui::End();
            return;
        }
        {
            bool hosting = m_server.IsHosting();
            bool connected = m_client.IsConnected() | m_client.IsConnecting();

            if (ImGui::Checkbox("Server", &hosting)) ToggleServer();
            if (ImGui::Checkbox("Client", &connected))
            {
                ToggleClient();
            }
            if (ImGui::IsItemActive())
            {
                ImGui::Text("Hello");
            }
        }
        ImGui::End();
    }
};

/*
class ExampleLayer3D : public sh::Layer
{
public:
    ExampleLayer3D()
        : sh::Layer("Example Layer")
        , fov(glm::radians(45.f))
        , m_camera(fov, 16.f/9.f)
    {}
    
    float fov;

    sh::PerspectiveCameraController m_camera;

    bool m_lookAtCube = false;
    sh::Transform m_transform;
    std::shared_ptr<sh::VertexArray> m_cube;
    std::shared_ptr<sh::Shader> m_shader;

    virtual void OnAttach() override
    {
        ImGui::SetCurrentContext(sh::ImGuiLayer::GetContext());

        // Create a cube
        {
            //https://developer.mozilla.org/en-US/docs/Web/API/WebGL_API/Tutorial/Creating_3D_objects_using_WebGL
            constexpr float positions[] {
            // Front face
                -1.0, -1.0, 1.0,
                1.0, -1.0, 1.0,
                1.0, 1.0, 1.0,
                -1.0, 1.0, 1.0,

                // Back face
                -1.0, -1.0, -1.0,
                -1.0, 1.0, -1.0,
                1.0, 1.0, -1.0,
                1.0, -1.0, -1.0,

                // Top face
                -1.0, 1.0, -1.0,
                -1.0, 1.0, 1.0,
                1.0, 1.0, 1.0,
                1.0, 1.0, -1.0,

                // Bottom face
                -1.0, -1.0, -1.0,
                1.0, -1.0, -1.0,
                1.0, -1.0, 1.0,
                -1.0, -1.0, 1.0,

                // Right face
                1.0, -1.0, -1.0,
                1.0, 1.0, -1.0,
                1.0, 1.0, 1.0,
                1.0, -1.0, 1.0,

                // Left face
                -1.0, -1.0, -1.0,
                -1.0, -1.0, 1.0,
                -1.0, 1.0, 1.0,
                -1.0, 1.0, -1.0,
        };
            constexpr uint32_t indices[] {
                0, 1, 2, 0, 2, 3,    // front
                4, 5, 6, 4, 6, 7,    // back
                8, 9, 10, 8, 10, 11,   // top
                12, 13, 14, 12, 14, 15,   // bottom
                16, 17, 18, 16, 18, 19,   // right
                20, 21, 22, 20, 22, 23,   // left
        };

            m_shader = sh::Shader::Create("res/shaders/Texture3D.glsl");

            auto vBuffer = sh::VertexBuffer::Create(positions, sizeof(positions));
            vBuffer->AddElement(sh::BufferElement(sh::ShaderDataType::Float3, "aPos"));

            auto iBuffer = sh::IndexBuffer::Create(indices, sizeof(indices) / sizeof(indices[0]));

            m_cube = sh::VertexArray::Create();
            m_cube->AddVertexBuffer(vBuffer);
            m_cube->SetIndexBuffer(iBuffer);
        }
        m_transform.SetPosition(glm::vec3(0, 0, 10));

        m_camera.GetCamera().transform.LookTowards(sh::Transform::GetWorldForward());
    }

    virtual void OnDetach() override { SH_INFO("Detached {0}", GetName()); }

    virtual void OnEvent(sh::Event& event) override
    {
        m_camera.OnEvent(event);

        sh::EventDispatcher d(event);
        d.Dispatch<sh::KeyPressedEvent>([&](sh::KeyPressedEvent& e)
        {
            if (e.GetKeyCode() == sh::KeyCode::Escape)
                sh::Application::Get().Exit();
            return false;
        });
    }

    virtual void OnUpdate(sh::Timestep ts) override final
    {
        m_camera.OnUpdate(ts);
        static float et = 0.f;
        et += ts;

        // Update camera
        const float rotX = glm::sin(glm::radians(et * 180.f));
        const float rotY = glm::cos(glm::radians(et * 90.f));
        const float rotZ = glm::sin(glm::cos(glm::radians(et * 180.f)));
        const float xOffset = glm::cos(glm::radians(et * 180.f));
        const float yOffset = glm::sin(glm::radians(et * 180.f));
        const glm::vec3 offset = glm::vec3(xOffset, yOffset, 0.f) * 3.f; 
        auto transform =
            glm::translate(glm::mat4(1), offset+ sh::Transform::GetWorldForward() * 10.f) *
            glm::rotate(glm::mat4(1), rotX, glm::vec3(1, 0, 0)) *
            glm::rotate(glm::mat4(1), rotY, glm::vec3(0, 1, 0)) *
            glm::rotate(glm::mat4(1), rotZ, glm::vec3(0, 0, 1))
            ;

        m_transform.SetPosition(transform[3]);

        if(m_lookAtCube)
            m_camera.GetCamera().transform.LookAt(m_transform.GetPosition());

        sh::Renderer::BeginScene(m_camera.GetCamera());
        sh::Renderer::Submit(m_shader, m_cube, m_transform.GetWorldMatrix());
        sh::Renderer::EndScene();
    }

    virtual void OnGuiRender() override final
    {
        if (!ImGui::Begin("Camera"))
        {
            ImGui::End();
            return;
        }

        ImGui::SliderAngle("fov", &fov, 0, 180);
        m_camera.GetCamera().SetFOV(fov);

        glm::vec3 pos(m_camera.GetCamera().transform.GetPosition());
        ImGui::DragFloat3("position", &pos.x);
        
        {
            glm::vec3 t(m_camera.GetCamera().transform.GetEulerRotation());
            ImGui::DragFloat3("rotation", &sh::Degrees(t).x);
            m_camera.GetCamera().transform.SetRotation(sh::Radians(t));
        }

        {
            glm::vec3 t(m_transform.GetEulerRotation());
            ImGui::DragFloat3("cube rotation", &sh::Degrees(t).x);
            m_transform.SetRotation(sh::Radians(t));
        }

        if (ImGui::Button("reset")) m_transform.SetRotation(glm::vec3(0));
        if (ImGui::Checkbox("look at cube", &m_lookAtCube)); // Yea we don't do anything here cuz we check in update

        ImGui::End();
    }
};
*/

std::unique_ptr<sh::Application> sh::CreateApplication()
{
    auto app = std::make_unique<sh::Application>();
    app->GetLayerStack().PushLayer(new ExampleLayer2D);

    return app;
}