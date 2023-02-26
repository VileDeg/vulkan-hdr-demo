#pragma once

struct Camera {
    static constexpr glm::vec3 sWorldUp{ 0.f,1.f,0.f };

    glm::vec3 front = glm::vec3{ 0,0,1 };
    glm::vec3 up = sWorldUp;
    glm::vec3 right = glm::vec3{ 1,0,0 };

    glm::vec3 pos{0.f, 0.f, -5.f};
    //glm::vec3 rot{};
    float yaw{90.f}; //rotation around y axis
    float pitch{}; //rotation around x axis

    float movSpeed = 10.f;
    float rotSpeed = 1.f;

    void goFront(float dt) {
        pos += front * movSpeed * dt;
    }

    void goBack(float dt) {
        pos -= front * movSpeed * dt;
    }

    void goLeft(float dt) {
        pos -= right * movSpeed * dt;
    }

    void goRight(float dt) {
        pos += right * movSpeed * dt;
    }

    void goUp(float dt) {
        pos += up * movSpeed * dt;
    }

    void goDown(float dt) {
        pos -= up * movSpeed * dt;
    }

    void rotate(float x, float y) {
        yaw -= x * rotSpeed;
        pitch -= y * rotSpeed;

        pitch = std::clamp(pitch, -90.f, 90.f);
        
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);

        right = glm::normalize(glm::cross(front, sWorldUp));
        up = glm::normalize(glm::cross(right, front));

        //std::cout << V3PR(front) << V3PR(right) << V3PR(up) << std::endl;
    }

    glm::mat4 getViewMat() {
        return glm::lookAt(pos, pos + front, up);
    }
};
