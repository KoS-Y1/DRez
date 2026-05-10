//
// Created by y1 on 2025-11-14.
//

#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace {
constexpr float kNear{0.01f};
constexpr float kFar{10000.0f};

constexpr float kPitchBound{89.0f};

constexpr float kMaxFov{179.0f};
constexpr float kMinFov{0.01f};

constexpr float kSensitivity{0.1f};
constexpr float kSpeed{0.01f};

constexpr XMFLOAT3 kWorldUpFloat{0.0f, 1.0f, 0.0f};
} // namespace

void Camera::Reset() {
    m_eye = XMFLOAT3(0.0f, 0.0f, 0.0f);

    m_yaw   = kDefaultYaw;
    m_pitch = kDefaultPitch;

    m_fov   = kDefaultFov;
    m_ratio = kDefaultRatio;

    m_speed       = kSpeed;
    m_sensitivity = kSensitivity;

    UpdateViewMatrix();
    UpdateProjectionMatrix();
}

void Camera::ProcessMovement(const XMFLOAT3 &direction) {
    // TODO: right now only handles keyboard input with one certain direction.
    // Controller stick can have linear input between two directions

    const CameraFrame frame   = CalculateCameraFrame();
    const XMVECTOR    worldUp = XMLoadFloat3(&kWorldUpFloat);

    XMVECTOR eye = XMLoadFloat3(&m_eye);

    if (direction.z == -1.0f) {
        eye = XMVectorAdd(eye, XMVectorScale(frame.forward, m_speed));
    }

    if (direction.z == 1.0f) {
        eye = XMVectorSubtract(eye, XMVectorScale(frame.forward, m_speed));
    }

    if (direction.x == 1.0f) {
        eye = XMVectorAdd(eye, XMVectorScale(frame.right, m_speed));
    }

    if (direction.x == -1.0f) {
        eye = XMVectorSubtract(eye, XMVectorScale(frame.right, m_speed));
    }

    if (direction.y == 1.0f) {
        eye = XMVectorAdd(eye, XMVectorScale(worldUp, m_speed));
    }

    if (direction.y == -1.0f) {
        eye = XMVectorSubtract(eye, XMVectorScale(worldUp, m_speed));
    }

    XMStoreFloat3(&m_eye, eye);
    UpdateViewMatrix();
}

void Camera::ProcessRotation(const XMFLOAT2 &offset) {
    m_yaw   += offset.x * m_sensitivity;
    m_pitch -= offset.y * m_sensitivity;

    m_pitch = std::clamp(m_pitch, -kPitchBound, kPitchBound);

    m_yaw = std::fmod(m_yaw, 360.0f);

    UpdateViewMatrix();
}

void Camera::ProcessZoom(float offset) {
    m_fov = std::clamp(m_fov - offset, kMinFov, kMaxFov);

    UpdateProjectionMatrix();
}

void Camera::SetRatio(float ratio) {
    m_ratio = ratio;
    UpdateProjectionMatrix();
}

void Camera::SetLocation(const XMFLOAT3 &location) {
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
    const XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(XMConvertToRadians(m_fov), m_ratio, kNear, kFar);
    XMStoreFloat4x4(&m_projection, projectionMatrix);
}

void Camera::UpdateViewMatrix() {
    const CameraFrame frame  = CalculateCameraFrame();
    const XMVECTOR    eye    = XMLoadFloat3(&m_eye);
    const XMVECTOR    target = XMVectorAdd(eye, frame.forward);
    const XMMATRIX    view   = XMMatrixLookAtRH(eye, target, frame.up);
    XMStoreFloat4x4(&m_view, view);
}

Camera::CameraFrame Camera::CalculateCameraFrame() const {
    const float yawRadian   = XMConvertToRadians(m_yaw);
    const float pitchRadian = XMConvertToRadians(m_pitch);

    XMFLOAT3 forwardF;
    forwardF.x = std::cos(yawRadian) * std::cos(pitchRadian);
    forwardF.y = std::sin(pitchRadian);
    forwardF.z = std::sin(yawRadian) * std::cos(pitchRadian);

    const XMVECTOR worldUp = XMLoadFloat3(&kWorldUpFloat);
    const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&forwardF));
    const XMVECTOR right   = XMVector3Normalize(XMVector3Cross(forward, worldUp));
    const XMVECTOR up      = XMVector3Normalize(XMVector3Cross(right, forward));

    return {forward, up, right};
}
