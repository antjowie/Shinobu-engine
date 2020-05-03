#include "Shinobu/Renderer/PerspectiveCameraController.h"

#include "Shinobu/Core/Input/Input.h"
#include "Shinobu/Core/Input/KeyCodes.h"

namespace sh
{
    PerspectiveCameraController::PerspectiveCameraController(float fovYRadians, float aspectRatio, float zNear, float zFar)
        : m_camera(fovYRadians, aspectRatio,zNear,zFar)
    {

    }

    void PerspectiveCameraController::OnUpdate(Timestep ts)
    {
        glm::vec3 offset(0);
        const glm::vec3 forward(m_camera.transform.GetForward());
        const glm::vec3 right(m_camera.transform.GetRight());
        const glm::vec3 up(m_camera.transform.GetUp());

        // NOTE: I flipped the forward for the camera. This is because OpenGL by default 'looks' into the -z
        // direction. Since I reuse the transform class and don't want to modify too many things, I change 
        // the direction for the camera. This is something to keep in mind. It should not affect anything else
        // and we can continue on as we like to.
        if (Input::IsKeyPressed(KeyCode::W)) offset += -forward;
        if (Input::IsKeyPressed(KeyCode::A)) offset += -right;
        if (Input::IsKeyPressed(KeyCode::S)) offset += forward;
        if (Input::IsKeyPressed(KeyCode::D)) offset += right;
        if (Input::IsKeyPressed(KeyCode::E)) offset += up;
        if (Input::IsKeyPressed(KeyCode::Q)) offset += -up;

        m_camera.transform.SetPosition(m_camera.transform.GetPosition() + offset * ts.Seconds() * 5.f);
    }

    void PerspectiveCameraController::OnEvent(Event& e)
    {
    }
}