function on_create()
    print("Script attached to entity ID: " .. tostring(entity:GetUUID()))
end

function on_update(dt)
    local transform = entity:get_component(Transform)
    local animator = entity:get_component(Animator)

    if not transform or not animator then
        return
    end

    if Input.is_key_down(Key.Space) then
        animator:set_bool("is_animating", true)
    else
        animator:set_bool("is_animating", false)
    end
end
