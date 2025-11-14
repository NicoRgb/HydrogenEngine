function on_create(entity)
    print("Entity created:", entity:GetUUID())
end

function on_update(entity, dt)
    local transform = entity:GetTransform()

    local pos = transform.pos
    pos.x = pos.x + dt * 2.0
    transform.pos = pos
end
