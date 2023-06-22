#pragma once

struct GLFWwindow;

class Camera 
{
public:
    void Update(GLFWwindow* window);
    void MouseInput(float x, float y) {
        rotate(x, y);
    }

    glm::vec3 GetPos() const {
        return _pos;
    }

    glm::mat4 GetViewMat() {
        return glm::lookAt(_pos, _pos + _front, _up);
    }

    glm::mat4 GetProjMat(float fovY, int w, int h);

    float GetDeltaTime() { return _dt; }
    
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
        _pos += sWorldUp * _movSpeed * dt;
    }

    void goDown(float dt) {
        _pos -= sWorldUp * _movSpeed * dt;
    }

    void rotate(float x, float y);

    void calculateFPS();
    void calculateDeltaTime();
private:
    static constexpr glm::vec3 sWorldUp{ 0.f, 1.f, 0.f };

    glm::vec3 _front{ 0.f, 0.f, 1.f };
    glm::vec3 _up{ sWorldUp };
    glm::vec3 _right{ -1.f, 0.f, 0.f };

    glm::vec3 _pos{0.f, 0.f, 0.f};

    float _yaw{90.f}; //rotation around y axis
    float _pitch{}; //rotation around x axis

    float _movSpeed{ 10.f };
    float _rotSpeed{ 1.f };
    float _sprintBoost{ 2.5f };

    int _fps{};
    float _dt{};

    int _oldWinWidth{ 0 };
    int _oldWinHeight{ 0 };
    float _oldFovY{ 0.f };

    glm::mat4 _oldProjMat;

};
