#include "Pedestrian.hpp"
#include "Renderer.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include <glm/geometric.hpp>
#include <iostream>
#include <cmath>

Pedestrian::Pedestrian()
    : m_sharedAnimation(nullptr)
    , m_animationTime(0.0f)
    , m_tileGrid(nullptr)
    , m_speed(2.0f)  // Slower than player
    , m_baseSpeed(2.0f)
    , m_panicSpeed(5.0f)
    , m_size(0.8f, 0.8f)
    , m_walkingDirection(SidewalkDirection::NorthSouth)
    , m_state(PedestrianState::Walking)
    , m_stateTimer(0.0f)
    , m_carjackFrame(0)
    , m_deathFrame(0)
    , m_deathAnimTimer(0.0f)
    , m_targetSidewalkPos(0.0f)
    , m_hasTargetSidewalk(false)
    , m_sidewalkCenterPos(0.0f)
    , m_pendingSidewalkDir(SidewalkDirection::None)
    , m_panicSource(0.0f)
    , m_panicDuration(0.0f) {
}

void Pedestrian::initialize(SpriteAnimation* sharedAnimation) {
    m_sharedAnimation = sharedAnimation;
    
    if (m_sharedAnimation) {
        // Calculate aspect ratio from frame dimensions for proper sprite size
        float aspectRatio = static_cast<float>(m_sharedAnimation->getFrameWidth()) / 
                           static_cast<float>(m_sharedAnimation->getFrameHeight());
        m_size = glm::vec2(0.8f * aspectRatio, 0.8f); // Slightly smaller than player
        
        // Randomize starting animation time so pedestrians aren't all in sync
        m_animationTime = static_cast<float>(rand()) / RAND_MAX * 0.8f;
    }
}

void Pedestrian::setSpeed(float speed) {
    m_baseSpeed = speed;
    m_panicSpeed = speed * 2.5f;
    if (m_state != PedestrianState::Panic) {
        m_speed = speed;
    }
}

bool Pedestrian::isBlockedByVehicle(const glm::vec3& position) const {
    if (!m_vehicleBlockCheck) return false;
    float pedRadius = std::max(m_size.x, m_size.y) * 0.3f;
    return m_vehicleBlockCheck(position, pedRadius);
}

void Pedestrian::snapToSurface(glm::vec3& pos) const {
    if (!m_tileGrid) {
        return;
    }
    pos.z = m_tileGrid->getSurfaceHeight(pos.x, pos.y, m_position.z);
}

void Pedestrian::setWalkingDirection(SidewalkDirection dir, bool avoidReverse) {
    m_walkingDirection = dir;
    
    // Set rotation based on direction
    // Since sidewalks are bidirectional, pick the appropriate direction
    float currentHeading = m_rotation.z;
    float option1, option2;
    
    switch (dir) {
        case SidewalkDirection::NorthSouth:
            option1 = 90.0f;   // North
            option2 = 270.0f;  // South
            break;
        case SidewalkDirection::EastWest:
            option1 = 0.0f;    // East
            option2 = 180.0f;  // West
            break;
        case SidewalkDirection::NorthEastSouthWest:
            option1 = 45.0f;   // NE
            option2 = 225.0f;  // SW
            break;
        case SidewalkDirection::NorthWestSouthEast:
            option1 = 135.0f;  // NW
            option2 = 315.0f;  // SE
            break;
        default:
            // No specific direction, keep current heading
            return;
    }
    
    // Pick the option based on current heading
    float diff1 = std::abs(Heading::shortestAngleDeltaDeg(currentHeading, option1));
    float diff2 = std::abs(Heading::shortestAngleDeltaDeg(currentHeading, option2));
    
    if (avoidReverse) {
        // When arriving from seeking sidewalk, pick the direction that continues forward
        // (i.e., the one closest to current heading, which was pointing towards the sidewalk)
        // But if that would be more than 90 degrees turn, pick the other one
        if (diff1 <= 90.0f) {
            m_rotation.z = option1;
        } else if (diff2 <= 90.0f) {
            m_rotation.z = option2;
        } else {
            // Both options are more than 90 degrees - just pick the closest
            m_rotation.z = (diff1 <= diff2) ? option1 : option2;
        }
    } else {
        // Normal case - pick the closest direction
        m_rotation.z = (diff1 <= diff2) ? option1 : option2;
    }
}

void Pedestrian::update(float deltaTime) {
    if (!m_active) return;
    
    // Handle different states
    switch (m_state) {
        case PedestrianState::Dead:
            updateDeathAnimation(deltaTime);
            return;
            
        case PedestrianState::CarjackExit:
        case PedestrianState::CarjackStunned:
        case PedestrianState::CarjackRecovery:
            updateCarjackAnimation(deltaTime);
            return;
            
        case PedestrianState::SeekingSidewalk:
            updateSeekingSidewalk(deltaTime);
            // Update animation time for walking
            m_animationTime += deltaTime;
            return;
            
        case PedestrianState::CenteringSidewalk:
            updateCenteringSidewalk(deltaTime);
            // Update animation time for walking
            m_animationTime += deltaTime;
            return;

        case PedestrianState::Panic:
            updatePanic(deltaTime);
            m_animationTime += deltaTime;
            return;
            
        case PedestrianState::Walking:
        default:
            break;
    }
    
    // Update per-instance animation timer
    m_animationTime += deltaTime;
    
    // Update movement
    updateMovement(deltaTime);
}

void Pedestrian::updateMovement(float deltaTime) {
    if (!m_tileGrid) return;
    
    // Move in the direction the pedestrian is facing
    glm::vec2 forward = Heading::forwardFromHeadingDeg(m_rotation.z);
    glm::vec3 delta(forward.x * m_speed * deltaTime, forward.y * m_speed * deltaTime, 0.0f);
    
    glm::vec3 newPosition = m_position + delta;
    
    // Check if the new position is still on a sidewalk
    glm::ivec3 currentGrid = m_tileGrid->worldToGrid(m_position);
    glm::ivec3 newGrid = m_tileGrid->worldToGrid(newPosition);
    
    // If we're moving to a new tile, check if it's still a sidewalk
    if (newGrid != currentGrid) {
        if (!m_tileGrid->isSidewalkTile(newPosition)) {
            // Not a sidewalk - turn around
            m_rotation.z = Heading::wrapDegrees360(m_rotation.z + 180.0f);
            return;
        }
        // Note: We don't change direction based on the new tile's sidewalk direction
        // Pedestrians keep walking in their current direction until they hit a dead end
    }
    
    // Check if we can move (wall collision)
    if (m_tileGrid->canOccupy(m_position, newPosition)) {
        // Check if a vehicle is blocking the way
        if (isBlockedByVehicle(newPosition)) {
            // Vehicle in the way - turn around
            m_rotation.z = Heading::wrapDegrees360(m_rotation.z + 180.0f);
            return;
        }
        snapToSurface(newPosition);
        m_position = newPosition;
    } else {
        // Hit a wall - turn around
        m_rotation.z = Heading::wrapDegrees360(m_rotation.z + 180.0f);
    }
}

void Pedestrian::kill() {
    if (m_state == PedestrianState::Dead) return;
    
    m_state = PedestrianState::Dead;
    m_deathFrame = 0;
    m_deathAnimTimer = 0.0f;
    m_speed = 0.0f;  // Stop moving
}

void Pedestrian::startPanic(const glm::vec3& threatPosition, float durationSeconds) {
    if (m_state == PedestrianState::Dead || isCarjacking()) {
        return;
    }

    m_state = PedestrianState::Panic;
    m_stateTimer = 0.0f;
    m_panicDuration = durationSeconds;
    m_panicSource = threatPosition;
    m_speed = m_panicSpeed;
}

void Pedestrian::updateDeathAnimation(float deltaTime) {
    const float deathFrameDuration = 0.15f;
    
    m_deathAnimTimer += deltaTime;
    
    // Advance to frame 1 (the final death frame) after the duration
    if (m_deathFrame == 0 && m_deathAnimTimer >= deathFrameDuration) {
        m_deathFrame = 1;
        // Stay at frame 1 (frame 10 in the sprite sheet)
    }
}

glm::vec4 Pedestrian::getCurrentFrameUV() const {
    if (!m_sharedAnimation) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    
    int frameIndex;
    
    if (m_state == PedestrianState::Dead) {
        // Death animation uses frames 8 and 9 (0-indexed, so frame 9 and 10 in 1-indexed)
        // Frame 8 = x position 4096, Frame 9 = x position 4608
        frameIndex = 8 + m_deathFrame;  // 8 or 9
    } else if (m_state == PedestrianState::CarjackExit || 
               m_state == PedestrianState::CarjackRecovery) {
        // Carjack exit uses frame 9 (0-indexed = 8)
        frameIndex = 8;
    } else if (m_state == PedestrianState::CarjackStunned) {
        // Carjack stunned uses frame 11 (0-indexed = 10)
        frameIndex = 10;
    } else {
        // Calculate frame based on this pedestrian's animation time
        // Assuming 8 frames at 0.1s each = 0.8s cycle
        const float frameDuration = 0.1f;
        const int numFrames = 8;
        const float animCycle = frameDuration * numFrames;
        
        float wrappedTime = fmod(m_animationTime, animCycle);
        frameIndex = static_cast<int>(wrappedTime / frameDuration) % numFrames;
    }
    
    int frameWidth = m_sharedAnimation->getFrameWidth();
    int frameHeight = m_sharedAnimation->getFrameHeight();
    
    // Get texture dimensions from the animation
    const Texture* tex = m_sharedAnimation->getTexture();
    if (!tex) return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    
    float texWidth = static_cast<float>(tex->getWidth());
    float texHeight = static_cast<float>(tex->getHeight());
    
    // Calculate UV for this frame (frames are horizontal)
    float u = (frameIndex * frameWidth) / texWidth;
    float v = 0.0f;
    float uScale = frameWidth / texWidth;
    float vScale = frameHeight / texHeight;
    
    return glm::vec4(u, v, uScale, vScale);
}

void Pedestrian::render(Renderer* renderer) {
    if (!m_active || !renderer) return;
    
    if (m_sharedAnimation && m_sharedAnimation->getTexture()) {
        glm::vec4 uvOffsetScale = getCurrentFrameUV();
        renderer->renderAnimatedSprite(*m_sharedAnimation->getTexture(),
                                       m_position,
                                       m_size, uvOffsetScale, m_rotation.z, glm::vec3(1.0f));
    }
}

void Pedestrian::startCarjackExit() {
    m_state = PedestrianState::CarjackExit;
    m_stateTimer = 0.0f;
    m_carjackFrame = 0;
    m_speed = 0.0f;  // Stop moving during carjack animation
}

bool Pedestrian::isCarjacking() const {
    return m_state == PedestrianState::CarjackExit ||
           m_state == PedestrianState::CarjackStunned ||
           m_state == PedestrianState::CarjackRecovery;
}

void Pedestrian::updateCarjackAnimation(float deltaTime) {
    const float exitFrameDuration = 0.15f;    // Duration to show frame 9 initially
    const float stunnedDuration = 2.0f;       // Duration to stay stunned at frame 11
    const float recoveryFrameDuration = 0.15f; // Duration to show frame 9 again
    
    m_stateTimer += deltaTime;
    
    switch (m_state) {
        case PedestrianState::CarjackExit:
            // Show frame 9, then transition to frame 11 (stunned)
            if (m_stateTimer >= exitFrameDuration) {
                m_state = PedestrianState::CarjackStunned;
                m_stateTimer = 0.0f;
            }
            break;
            
        case PedestrianState::CarjackStunned:
            // Stay at frame 11 for a couple seconds
            if (m_stateTimer >= stunnedDuration) {
                m_state = PedestrianState::CarjackRecovery;
                m_stateTimer = 0.0f;
            }
            break;
            
        case PedestrianState::CarjackRecovery:
            // Show frame 9 again, then become normal pedestrian
            if (m_stateTimer >= recoveryFrameDuration) {
                // Check if we're on a road and need to find a sidewalk
                if (m_tileGrid && m_tileGrid->isRoadTile(m_position)) {
                    findNearestSidewalk();
                    if (m_hasTargetSidewalk) {
                        m_state = PedestrianState::SeekingSidewalk;
                    } else {
                        // No sidewalk found, just start walking
                        m_state = PedestrianState::Walking;
                    }
                } else {
                    // Already on sidewalk or no tile grid, just start walking
                    m_state = PedestrianState::Walking;
                }
                m_stateTimer = 0.0f;
                m_speed = 2.0f;  // Resume normal speed
            }
            break;
            
        default:
            break;
    }
}

void Pedestrian::updatePanic(float deltaTime) {
    if (!m_tileGrid) {
        return;
    }

    m_stateTimer += deltaTime;
    if (m_stateTimer >= m_panicDuration) {
        m_state = PedestrianState::SeekingSidewalk;
        m_stateTimer = 0.0f;
        m_speed = m_baseSpeed;
        findNearestSidewalk();
        return;
    }

    glm::vec2 away(m_position.x - m_panicSource.x, m_position.y - m_panicSource.y);
    if (away.x == 0.0f && away.y == 0.0f) {
        away = glm::vec2(1.0f, 0.0f);
    }

    away = glm::normalize(away);

    const float candidateAngles[] = {0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
    glm::vec2 bestDir = away;
    float bestScore = -1.0f;

    for (float angleDeg : candidateAngles) {
        const float angleRad = glm::radians(angleDeg);
        glm::vec2 candidate(
            away.x * std::cos(angleRad) - away.y * std::sin(angleRad),
            away.x * std::sin(angleRad) + away.y * std::cos(angleRad));

        glm::vec3 delta(candidate.x * m_speed * deltaTime, candidate.y * m_speed * deltaTime, 0.0f);
        glm::vec3 newPosition = m_position + delta;
        if (!m_tileGrid->canOccupy(m_position, newPosition)) {
            continue;
        }
        // Skip directions blocked by a vehicle
        if (isBlockedByVehicle(newPosition)) {
            continue;
        }

        const float score = glm::dot(candidate, away);
        if (score > bestScore) {
            bestScore = score;
            bestDir = candidate;
        }
    }

    glm::vec3 delta(bestDir.x * m_speed * deltaTime, bestDir.y * m_speed * deltaTime, 0.0f);
    glm::vec3 newPosition = m_position + delta;
    if (m_tileGrid->canOccupy(m_position, newPosition) && !isBlockedByVehicle(newPosition)) {
        snapToSurface(newPosition);
        m_position = newPosition;
    }

    m_rotation.z = Heading::wrapDegrees360(Heading::headingDegFromForward(bestDir));
}

void Pedestrian::findNearestSidewalk() {
    if (!m_tileGrid) {
        m_hasTargetSidewalk = false;
        return;
    }
    
    glm::ivec3 currentGrid = m_tileGrid->worldToGrid(m_position);
    float tileSize = m_tileGrid->getTileSize();
    const glm::ivec3& gridSize = m_tileGrid->getGridSize();
    
    // The ground tile is at z-1 from where entities stand
    int groundZ = currentGrid.z - 1;
    
    // Search in expanding rings around the current position
    float nearestDistSq = std::numeric_limits<float>::max();
    glm::vec3 nearestPos(0.0f);
    bool found = false;
    
    const int maxSearchRadius = 10;  // Search up to 10 tiles away
    
    for (int radius = 1; radius <= maxSearchRadius; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                // Only check tiles on the edge of the current ring
                if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                
                int x = currentGrid.x + dx;
                int y = currentGrid.y + dy;
                
                if (x < 0 || x >= gridSize.x || y < 0 || y >= gridSize.y || groundZ < 0 || groundZ >= gridSize.z) continue;
                
                const Tile* tile = m_tileGrid->getTile(x, y, groundZ);
                if (!tile || !tile->isSidewalk()) continue;
                
                // Found a sidewalk tile - calculate world position (center of tile)
                glm::vec3 sidewalkPos(x * tileSize, y * tileSize, m_position.z);
                float distSq = glm::dot(glm::vec2(sidewalkPos.x - m_position.x, sidewalkPos.y - m_position.y), 
                                        glm::vec2(sidewalkPos.x - m_position.x, sidewalkPos.y - m_position.y));
                
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    nearestPos = sidewalkPos;
                    found = true;
                }
            }
        }
        
        // If we found at least one sidewalk in this ring, stop searching
        if (found) break;
    }
    
    if (found) {
        m_targetSidewalkPos = nearestPos;
        m_hasTargetSidewalk = true;
        
        // Set rotation to face the target
        glm::vec2 toTarget(m_targetSidewalkPos.x - m_position.x, m_targetSidewalkPos.y - m_position.y);
        if (glm::length(toTarget) > 0.01f) {
            toTarget = glm::normalize(toTarget);
            m_rotation.z = Heading::headingDegFromForward(toTarget);
        }
        
        std::cout << "Pedestrian at (" << m_position.x << ", " << m_position.y 
                  << ") seeking sidewalk at (" << m_targetSidewalkPos.x << ", " << m_targetSidewalkPos.y << ")" << std::endl;
    } else {
        m_hasTargetSidewalk = false;
        std::cout << "Pedestrian at (" << m_position.x << ", " << m_position.y << ") found no nearby sidewalk" << std::endl;
    }
}

void Pedestrian::updateSeekingSidewalk(float deltaTime) {
    if (!m_hasTargetSidewalk || !m_tileGrid) {
        // Lost target or no tile grid, switch to normal walking
        m_state = PedestrianState::Walking;
        return;
    }
    
    // Check if we've reached the sidewalk
    if (m_tileGrid->isSidewalkTile(m_position)) {
        // Reached a sidewalk, now walk to the center of the tile
        glm::ivec3 gridPos = m_tileGrid->worldToGrid(m_position);
        gridPos.z -= 1;  // Check the ground tile
        const Tile* tile = m_tileGrid->getTile(gridPos);
        if (tile) {
            m_pendingSidewalkDir = tile->getSidewalkDirection();
            // Calculate center of the sidewalk tile
            float tileSize = m_tileGrid->getTileSize();
            m_sidewalkCenterPos = glm::vec3(gridPos.x * tileSize, gridPos.y * tileSize, m_position.z);
            m_state = PedestrianState::CenteringSidewalk;
        } else {
            m_state = PedestrianState::Walking;
        }
        m_hasTargetSidewalk = false;
        return;
    }
    
    // Move towards the target sidewalk
    glm::vec2 toTarget(m_targetSidewalkPos.x - m_position.x, m_targetSidewalkPos.y - m_position.y);
    float distToTarget = glm::length(toTarget);
    
    // Update rotation to face target
    if (distToTarget > 0.01f) {
        glm::vec2 direction = glm::normalize(toTarget);
        m_rotation.z = Heading::headingDegFromForward(direction);
    }
    
    // Move towards target
    glm::vec2 forward = Heading::forwardFromHeadingDeg(m_rotation.z);
    glm::vec3 delta(forward.x * m_speed * deltaTime, forward.y * m_speed * deltaTime, 0.0f);
    glm::vec3 newPosition = m_position + delta;
    
    // Check if the new position would be on a sidewalk - if so, move there and transition
    if (m_tileGrid->isSidewalkTile(newPosition)) {
        snapToSurface(newPosition);
        m_position = newPosition;
        m_hasTargetSidewalk = false;
        
        // Now walk to the center of the tile
        glm::ivec3 gridPos = m_tileGrid->worldToGrid(newPosition);
        gridPos.z -= 1;
        const Tile* tile = m_tileGrid->getTile(gridPos);
        if (tile) {
            m_pendingSidewalkDir = tile->getSidewalkDirection();
            float tileSize = m_tileGrid->getTileSize();
            m_sidewalkCenterPos = glm::vec3(gridPos.x * tileSize, gridPos.y * tileSize, m_position.z);
            m_state = PedestrianState::CenteringSidewalk;
        } else {
            m_state = PedestrianState::Walking;
        }
        return;
    }
    
    // Check if we can move there
    if (m_tileGrid->canOccupy(m_position, newPosition)) {
        // Check if a vehicle is blocking the way
        if (isBlockedByVehicle(newPosition)) {
            findNearestSidewalk();
            if (!m_hasTargetSidewalk) {
                m_state = PedestrianState::Walking;
            }
            return;
        }
        snapToSurface(newPosition);
        m_position = newPosition;
    } else {
        // Can't move due to wall - try to find an alternate path or give up
        // For now, just recalculate the nearest sidewalk
        findNearestSidewalk();
        if (!m_hasTargetSidewalk) {
            // No sidewalk found, give up and just walk
            m_state = PedestrianState::Walking;
        }
    }
    
    // If we've been seeking too long or overshot the target, recalculate
    if (distToTarget < 0.5f) {
        // We're close but not on sidewalk yet - keep going, don't give up
        // The sidewalk tile check above should catch us soon
    }
}

void Pedestrian::updateCenteringSidewalk(float deltaTime) {
    if (!m_tileGrid) {
        m_state = PedestrianState::Walking;
        return;
    }
    
    // Calculate distance to center
    glm::vec2 toCenter(m_sidewalkCenterPos.x - m_position.x, m_sidewalkCenterPos.y - m_position.y);
    float distToCenter = glm::length(toCenter);
    
    // If we're close enough to the center, start normal walking
    const float centerThreshold = 0.3f;
    if (distToCenter < centerThreshold) {
        m_state = PedestrianState::Walking;
        setWalkingDirection(m_pendingSidewalkDir, true);
        return;
    }
    
    // Update rotation to face center
    if (distToCenter > 0.01f) {
        glm::vec2 direction = glm::normalize(toCenter);
        m_rotation.z = Heading::headingDegFromForward(direction);
    }
    
    // Move towards center
    glm::vec2 forward = Heading::forwardFromHeadingDeg(m_rotation.z);
    float moveDistance = m_speed * deltaTime;
    
    // Don't overshoot the center
    if (moveDistance > distToCenter) {
        moveDistance = distToCenter;
    }
    
    glm::vec3 delta(forward.x * moveDistance, forward.y * moveDistance, 0.0f);
    glm::vec3 newPosition = m_position + delta;
    
    // Check if we can move there
    if (m_tileGrid->canOccupy(m_position, newPosition)) {
        if (isBlockedByVehicle(newPosition)) {
            // Vehicle blocking path to center, just start walking
            m_state = PedestrianState::Walking;
            setWalkingDirection(m_pendingSidewalkDir, true);
            return;
        }
        snapToSurface(newPosition);
        m_position = newPosition;
    } else {
        // Can't move to center, just start walking
        m_state = PedestrianState::Walking;
        setWalkingDirection(m_pendingSidewalkDir, true);
    }
}
