local speed = 5.0
local jumpForce = 4.0

local scriptData = {}

function on_create()
    Log.info("[CharacterController] Initializing script on entity...")
    
    if entity == nil then
        Log.error("[CharacterController] ERROR: 'entity' is nil!")
        return
    end

    if not entity:has_component("Rigidbody") then
        Log.warn("[CharacterController] WARNING: Entity does not have a Rigidbody component!")
        return
    end

    scriptData.rb = entity:get_component("Rigidbody")
    if scriptData.rb then
        Log.info("[CharacterController] Successfully acquired Rigidbody component.")
    else
        Log.error("[CharacterController] ERROR: Failed to get Rigidbody component.")
    end
end

function on_update(dt)
    if not scriptData.rb then return end

    local vx, vy, vz = scriptData.rb:GetLinearVelocity()
    
    local moveX = 0.0
    local moveZ = 0.0

    if Input.is_key_down(Key.W) then
        moveZ = -1.0
    elseif Input.is_key_down(Key.S) then
        moveZ = 1.0
    end

    if Input.is_key_down(Key.A) then
        moveX = -1.0
    elseif Input.is_key_down(Key.D) then
        moveX = 1.0
    end

    local length = math.sqrt(moveX * moveX + moveZ * moveZ)
    if length > 0 then
        moveX = (moveX / length) * speed
        moveZ = (moveZ / length) * speed
    else
        vx = vx * 0.9
        vz = vz * 0.9
    end

    scriptData.rb:SetLinearVelocity(moveX, vy, moveZ)

    if Input.is_key_down(Key.Space) then
        if math.abs(vy) < 0.1 then
            Log.info("[CharacterController] Jump executed!")
            scriptData.rb:ApplyForceToCenter(0.0, jumpForce * 100.0, 0.0)
        end
    end
end
