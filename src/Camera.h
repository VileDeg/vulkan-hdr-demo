#pragma once

struct GLFWwindow;

class Camera 
{
public:
    void Update(GLFWwindow* window, float dt);
    void MouseInput(float x, float y) {
        rotate(x, y);
    }

    glm::mat4 GetViewMat() {
        return glm::lookAt(_pos, _pos + _front, _up);
    }
private:
    void goFront(float dt) {
        _pos += _front * _movSpeed * dt;
    }

    void goBack(float dt) {
        _pos -= _front * _movSpeed * dt;
    }

    void goLeft(float dt) {
        _pos -= _right * _movSpeed * dt;
    }

    void goRight(float dt) {
        _pos += _right * _movSpeed * dt;
    }

    void goUp(float dt) {
        _pos += _up * _movSpeed * dt;
    }

    void goDown(float dt) {
        _pos -= _up * _movSpeed * dt;
    }

    void rotate(float x, float y);
private:
    static constexpr glm::vec3 sWorldUp{ 0.f, 1.f, 0.f };

    glm::vec3 _front{ 0.f, 0.f, 1.f };
    glm::vec3 _up{ sWorldUp };
    glm::vec3 _right{ -1.f, 0.f, 0.f };

    glm::vec3 _pos{0.f, 0.f, -5.f};

    float _yaw{90.f}; //rotation around y axis
    float _pitch{}; //rotation around x axis

    float _movSpeed{ 10.f };
    float _rotSpeed{ 1.f };

    
};
