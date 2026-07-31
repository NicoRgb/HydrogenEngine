local third_person_camera = {
    properties = {
        target = { type = "entity", value = nil },
        distance = { type = "float", value = 6.0 },
        min_distance = { type = "float", value = 2.0 },
        max_distance = { type = "float", value = 15.0 },
        sensitivity = { type = "float", value = 0.1 },
        height_offset = { type = "float", value = 1.5 },
        smooth_speed = { type = "float", value = 10.0 }
    }
}

local cam_data = {
    yaw = 0.0,
    pitch = 20.0,
    current_position = vec3.new(0.0, 0.0, 0.0)
}

function third_person_camera:on_create()
    Log.info("[ThirdPersonCamera] Initialized Cinemachine-style orbit camera.")
    
    if not self.entity:has_component("Camera") then
        Log.error("[ThirdPersonCamera] Entity missing Camera component!")
        return
    end

    cam_data.cam = self.entity:get_component("Camera")
end

function third_person_camera:on_update(dt)
    local target_ent = self.properties.target.value
    if target_ent == nil or not target_ent:is_valid() or not target_ent:has_component("Transform") then
        Log.warn("[ThirdPersonCamera] Target Entity is invalid")
        return
    end

    -- 1. Handle Mouse Orbit Input (Right-click drag to rotate)
    if Input.is_mouse_button_down(Key.MouseRight) then
        local dx = Input.get_mouse_delta_x()
        local dy = Input.get_mouse_delta_y()

        cam_data.yaw = cam_data.yaw - (dx * self.properties.sensitivity.value)
        cam_data.pitch = cam_data.pitch + (dy * self.properties.sensitivity.value)

        -- Clamp pitch to prevent vertical flipping
        cam_data.pitch = math.max(-89.0, math.min(89.0, cam_data.pitch))
    end

    -- 2. Calculate Target Focus Position with height offset
    local target_transform = target_ent:get_component("Transform")
    local target_pos = target_transform.pos
    local focus_point = vec3.new(target_pos.x, target_pos.y + self.properties.height_offset.value, target_pos.z)

    -- 3. Convert Spherical Coordinates (Yaw/Pitch/Distance) to Cartesian Offset Vector
    local rad_yaw = math.rad(cam_data.yaw)
    local rad_pitch = math.rad(cam_data.pitch)

    local offset_x = self.properties.distance.value * math.cos(rad_pitch) * math.sin(rad_yaw)
    local offset_y = self.properties.distance.value * math.sin(rad_pitch)
    local offset_z = self.properties.distance.value * math.cos(rad_pitch) * math.cos(rad_yaw)

    local desired_cam_pos = vec3.new(
        focus_point.x + offset_x,
        focus_point.y + offset_y,
        focus_point.z + offset_z
    )

    -- 4. Smooth Damping (Lerp) towards desired position for fluidity
    local t = math.min(1.0, self.properties.smooth_speed.value * dt)
    cam_data.current_position.x = cam_data.current_position.x + (desired_cam_pos.x - cam_data.current_position.x) * t
    cam_data.current_position.y = cam_data.current_position.y + (desired_cam_pos.y - cam_data.current_position.y) * t
    cam_data.current_position.z = cam_data.current_position.z + (desired_cam_pos.z - cam_data.current_position.z) * t

    -- 5. Apply Position to Camera Transform
    local cam_transform = self.entity:get_component("Transform")
    cam_transform.pos = cam_data.current_position
end

return third_person_camera