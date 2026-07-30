local character_controller = {
    properties = {
        speed = {
            type = "float",
            value = 5.0
        },
        jump_force = {
            type = "float",
            value = 4.0
        }
    }
}

local script_data = {}

function character_controller:on_create()
    Log.info("[CharacterController] Initializing script on entity...")
    
    if self.entity == nil then
        Log.error("[CharacterController] ERROR: 'entity' is nil!")
        return
    end

    if not self.entity:has_component("Rigidbody") then
        Log.warn("[CharacterController] WARNING: Entity does not have a Rigidbody component!")
        return
    end

    script_data.rb = self.entity:get_component("Rigidbody")
    if script_data.rb then
        Log.info("[CharacterController] Successfully acquired Rigidbody component.")
    else
        Log.error("[CharacterController] ERROR: Failed to get Rigidbody component.")
    end
end

function character_controller:on_update(dt)
    if not script_data.rb then return end

    local current_speed = self.properties.speed.value
    local current_jump_force = self.properties.jump_force.value

    local vx, vy, vz = script_data.rb:GetLinearVelocity()
    
    local move_x = 0.0
    local move_z = 0.0

    if Input.is_key_down(Key.W) then
        move_z = -1.0
    elseif Input.is_key_down(Key.S) then
        move_z = 1.0
    end

    if Input.is_key_down(Key.A) then
        move_x = -1.0
    elseif Input.is_key_down(Key.D) then
        move_x = 1.0
    end

    local length = math.sqrt(move_x * move_x + move_z * move_z)
    if length > 0 then
        move_x = (move_x / length) * current_speed
        move_z = (move_z / length) * current_speed
    else
        vx = vx * 0.9
        vz = vz * 0.9
    end

    script_data.rb:SetLinearVelocity(move_x, vy, move_z)

    if Input.is_key_down(Key.Space) then
        if math.abs(vy) < 0.1 then
            Log.info("[CharacterController] Jump executed!")
            script_data.rb:ApplyForceToCenter(0.0, current_jump_force * 100.0, 0.0)
        end
    end
end

return character_controller
