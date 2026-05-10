//
// Created by y1 on 2025-11-14.
//

#pragma once

#include <string>
#include <string_view>

#include <directxmath.h>

class Camera {
public:
    static constexpr float kDefaultFov{60.0f};
    static constexpr float kDefaultYaw{-90.0f};
    static constexpr float kDefaultPitch{0.0f};
    static constexpr float kDefaultRatio{16.0f / 9.0f};

    Camera()
        : Camera("Camera") {}

    explicit Camera(std::string name)
        : m_name(std::move(name)) {
        Reset();
    }

    Camera(const Camera &)            = delete;
    Camera &operator=(const Camera &) = delete;

    Camera(Camera &&) noexcept            = default;
    Camera &operator=(Camera &&) noexcept = default;

    void Reset();

    void ProcessMovement(const DirectX::XMFLOAT3 &direction);
    void ProcessRotation(const DirectX::XMFLOAT2 &offset);
    void ProcessZoom(float offset);

    void SetRatio(float ratio);
    void SetLocation(const DirectX::XMFLOAT3 &location);
    void SetFov(float fov);
    void SetRotation(float yaw, float pitch);

    [[nodiscard]] DirectX::XMFLOAT4X4 GetViewMatrix() const { return m_view; }

    [[nodiscard]] DirectX::XMFLOAT4X4 GetProjectionMatrix() const { return m_projection; }

    [[nodiscard]] DirectX::XMFLOAT3 GetLocation() const { return m_eye; }

    [[nodiscard]] float GetYaw() const { return m_yaw; }

    [[nodiscard]] float GetPitch() const { return m_pitch; }

    [[nodiscard]] float GetFov() const { return m_fov; }

    [[nodiscard]] std::string_view GetName() const { return m_name; }


private:
    std::string m_name{};

    struct CameraFrame {
        DirectX::XMVECTOR forward;
        DirectX::XMVECTOR up;
        DirectX::XMVECTOR right;
    };

    DirectX::XMFLOAT4X4 m_view{};
    DirectX::XMFLOAT4X4 m_projection{};

    DirectX::XMFLOAT3 m_eye{};

    float m_yaw{};
    float m_pitch{};

    float m_fov{};
    float m_ratio{};

    float m_sensitivity{};
    float m_speed{};

    void UpdateViewMatrix();
    void UpdateProjectionMatrix();

    [[nodiscard]] CameraFrame CalculateCameraFrame() const;
};
