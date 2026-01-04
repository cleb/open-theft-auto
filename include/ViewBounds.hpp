#pragma once

#include "Camera.hpp"
#include <glm/glm.hpp>
#include <cmath>

// Shared view bounds structure and utility functions for spawn managers
struct ViewBounds {
    float minX, maxX;
    float minY, maxY;
    
    // Calculate view bounds from camera and projection info
    static ViewBounds calculate(const Camera* camera, float fovRadians, float aspectRatio) {
        ViewBounds bounds{0, 0, 0, 0};
        
        if (!camera) return bounds;
        
        const glm::vec3& cameraPos = camera->getPosition();
        const glm::vec3& target = camera->getTarget();
        
        // Calculate the camera height above the ground plane (Z=0)
        float cameraHeight = cameraPos.z;
        
        // For a perspective camera looking down at the ground:
        // The half-height of the visible area at distance d is: d * tan(fov/2)
        // The half-width is: halfHeight * aspectRatio
        float distanceToGround = cameraHeight;
        
        float halfFov = fovRadians / 2.0f;
        float halfHeight = distanceToGround * std::tan(halfFov);
        float halfWidth = halfHeight * aspectRatio;
        
        // The visible area is centered on the target position (where camera looks)
        bounds.minX = target.x - halfWidth;
        bounds.maxX = target.x + halfWidth;
        bounds.minY = target.y - halfHeight;
        bounds.maxY = target.y + halfHeight;
        
        return bounds;
    }
    
    // Check if a position is within the view bounds
    bool contains(const glm::vec3& position) const {
        return position.x >= minX && position.x <= maxX &&
               position.y >= minY && position.y <= maxY;
    }
    
    // Check if a position is in the spawn zone (outside view but within margin)
    bool isInSpawnZone(const glm::vec3& position, float viewMargin, float minSpawnOffset = 2.0f) const {
        float outerMinX = minX - viewMargin;
        float outerMaxX = maxX + viewMargin;
        float outerMinY = minY - viewMargin;
        float outerMaxY = maxY + viewMargin;
        
        float innerMinX = minX - minSpawnOffset;
        float innerMaxX = maxX + minSpawnOffset;
        float innerMinY = minY - minSpawnOffset;
        float innerMaxY = maxY + minSpawnOffset;
        
        bool inOuterZone = position.x >= outerMinX && position.x <= outerMaxX &&
                          position.y >= outerMinY && position.y <= outerMaxY;
        
        bool inInnerZone = position.x >= innerMinX && position.x <= innerMaxX &&
                          position.y >= innerMinY && position.y <= innerMaxY;
        
        return inOuterZone && !inInnerZone;
    }
    
    // Get expanded bounds for despawn checks
    ViewBounds expanded(float margin) const {
        return ViewBounds{
            minX - margin,
            maxX + margin,
            minY - margin,
            maxY + margin
        };
    }
};
