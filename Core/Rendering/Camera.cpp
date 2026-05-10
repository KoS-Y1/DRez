//
// Created by y1 on 2025-11-14.
//

#include "Camera.h"

#include "SceneUtils.h"

#include <algorithm>
#include <glm/detail/type_quat.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};

constexpr float kNear{0.01f};
constexpr float kFar{10000.0f};

constexpr float kPitchBound{89.0f};

constexpr float kMaxFov{179.0f};
constexpr float kMinFov{0.01f};

constexpr float kSensitivity{0.1f};
constexpr float kSpeed{0.01f};

} // namespace

Camera::Camera(std::string name, const CameraConfig &config, float ratio)
    : m_name(std::move(name))
    , m_eye(config.position)
    , m_yaw(config.yaw)
    , m_pitch(config.pitch)
    , m_fov(config.fov) {
    m_ratio = ratio;

    m_speed       = kSpeed;
    m_sensitivity = kSensitivity;

    UpdateViewMatrix();
    UpdateProjectionMatrix();
}

void Camera::Reset(float ratio) {
    m_eye = glm::vec3(0.0f, 0.0f, 0.0f);

    m_yaw   = kDefaultYaw;
    m_pitch = kDefaultPitch;

    m_fov   = kDefaultFov;
    m_ratio = ratio;

    m_speed       = kSpeed;
    m_sensitivity = kSensitivity;

    UpdateViewMatrix();
    UpdateProjectionMatrix();
}

void Camera::ProcessMovement(const glm::vec3 &direction) {
    // TODO: right now only handles keyboard input with one certain direction.
    // Controller stick can have linear input between two directions

    const CameraFrame frame = CalculateCameraFrame();

    if (direction == glm::vec3(0.0f, 0.0f, -1.0f)) {
        m_eye += frame.forward * m_speed;
    }

    if (direction == glm::vec3(0.0f, 0.0f, 1.0f)) {
        m_eye -= frame.forward * m_speed;
    }

    if (direction == glm::vec3(1.0f, 0.0f, 0.0f)) {
        m_eye += frame.right * m_speed;
    }

    if (direction == glm::vec3(-1.0f, 0.0f, 0.0f)) {
        m_eye -= frame.right * m_speed;
    }

    if (direction == glm::vec3(0.0f, 1.0f, 0.0f)) {
        m_eye += kWorldUp * m_speed;
    }

    if (direction == glm::vec3(0.0f, -1.0f, 0.0f)) {
        m_eye -= kWorldUp * m_speed;
    }

    UpdateViewMatrix();
}

void Camera::ProcessRotation(const glm::vec2 &offset) {
    m_yaw   += offset.x * m_sensitivity;
    m_pitch -= offset.y * m_sensitivity;

    m_pitch = std::clamp(m_pitch, -kPitchBound, kPitchBound);

    m_yaw = glm::mod(m_yaw, 360.f);

    UpdateViewMatrix();
}

void Camera::ProcessZoom(float offset) {
    float fov = m_fov - offset;

    fov   = glm::clamp(fov, kMinFov, kMaxFov);
    m_fov = fov;

    UpdateProjectionMatrix();
}

void Camera::SetRatio(float ratio) {
    m_ratio = ratio;
    UpdateProjectionMatrix();
}

void Camera::SetLocation(const glm::vec3 &location) {
    m_eye = location;
    UpdateViewMatrix();
}

void Camera::SetFov(float fov) {
    m_fov = fov;
    UpdateProjectionMatrix();
}

void Camera::SetRotation(float yaw, float pitch) {
    m_yaw   = yaw;
    m_pitch = pitch;

    UpdateViewMatrix();
}

void Camera::UpdateProjectionMatrix() {
    glm::mat4 projectionMatrix = glm::perspectiveRH_ZO(glm::radians(m_fov), m_ratio, kNear, kFar);

    // Flip the Y axis for Vulkan
    projectionMatrix[1][1] *= -1;

    m_projection = projectionMatrix;
}

void Camera::UpdateViewMatrix() {
    const CameraFrame frame = CalculateCameraFrame();
    m_view                  = glm::lookAt(m_eye, m_eye + frame.forward, frame.up);
}

Camera::CameraFrame Camera::CalculateCameraFrame() const {
    const float yawRadian   = glm::radians(m_yaw);
    const float pitchRadian = glm::radians(m_pitch);

    glm::vec3 forward;
    forward.x = cos(yawRadian) * cos(pitchRadian);
    forward.y = sin(pitchRadian);
    forward.z = sin(yawRadian) * cos(pitchRadian);
    forward   = glm::normalize(forward);

    const glm::vec3 right = glm::normalize(glm::cross(forward, kWorldUp));

    const glm::vec3 up = glm::normalize(glm::cross(right, forward));

    return {forward, up, right};
}