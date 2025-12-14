local type = type
local math = require("math")
local lstg = require("lstg")
local _New = lstg._New
function lstg.New(class, ...)
    local o, init = _New(class)
    if init then
        o[1][1](o, ...)
    end
    return o
end
local _Del = lstg._Del
function lstg.Del(o, ...)
    if _Del(o) then
        o[1][2](o, ...)
    end
end
local _Kill = lstg._Kill
function lstg.Kill(o, ...)
    if _Kill(o) then
        o[1][6](o, ...)
    end
end
local _UpdateListFirst = lstg._UpdateListFirst
local _UpdateListNext = lstg._UpdateListNext
local _DetectListFirst = lstg._DetectListFirst
local _DetectListNext = lstg._DetectListNext
local objects = lstg.ObjTable()
---@param group [0, 16]
---@param checking_world integer? optional world flag to check.
function lstg.ObjList(group, checking_world)
    if group < 0 or group >= 16 then
        local id = _UpdateListFirst(checking_world)
        return function()
            if id == 0 then
                return nil, nil
            else
                local i, o = id, objects[id]
                id = _UpdateListNext(id, checking_world)
                return i, o
            end
        end
    else
        local id = _DetectListFirst(group, checking_world)
        return function()
            if id == 0 then
                return nil, nil
            else
                local i, o = id, objects[id]
                id = _DetectListNext(group, id, checking_world)
                return i, o
            end
        end
    end
end
local _sin = lstg.sin
local _cos = lstg.cos
function lstg.SetV(o, v, a, update_rot)
    o.vx = v * _cos(a)
    o.vy = v * _sin(a)
    if update_rot then
        o.rot = a
    end
end
local sqrt = math.sqrt
local _atan2 = lstg.atan2
function lstg.GetV(o)
    local vx, vy = o.vx, o.vy
    return sqrt(vx * vx + vy * vy), _atan2(vy, vx)
end
local function _dxdy(a, b, c, d)
    if d then
        return c - a, d - b
    elseif type(c) == "number" then
        return b - a.x, c - a.y
    elseif c then
        return c.x - a, c.y - b
    else
        return b.x - a.x, b.y - a.y
    end
end
function lstg.Dist(a, b, c, d)
    local dx, dy = _dxdy(a, b, c, d)
    return sqrt(dx * dx + dy * dy)
end
function lstg.Angle(a, b, c, d)
    local dx, dy = _dxdy(a, b, c, d)
    return _atan2(dy, dx)
end
local apiIsSameWorld = lstg.IsSameWorld
function lstg.IsSameWorld(a, b)
    if type(a) == "number" and type(b) == "number" then
        apiIsSameWorld(a, b)
    else
        apiIsSameWorld(a.world, b.world)
    end
end

function lstg.Render3D(img, x, y, z, rotationx, rotationy, rotationz, scalex, scaley)
    local halfWidth = 0.5 * scalex
    local halfHeight = 0.5 * scaley

    local vertices = {
        {-halfWidth, -halfHeight, 0},
        { halfWidth, -halfHeight, 0},
        { halfWidth,  halfHeight, 0},
        {-halfWidth,  halfHeight, 0}
    }

    local function rotateX(v, angle)
        local rad = math.rad(angle)
        local y = v[2] * math.cos(rad) - v[3] * math.sin(rad)
        local z = v[2] * math.sin(rad) + v[3] * math.cos(rad)
        return {v[1], y, z}
    end

    local function rotateY(v, angle)
        local rad = math.rad(angle)
        local x = v[1] * math.cos(rad) + v[3] * math.sin(rad)
        local z = -v[1] * math.sin(rad) + v[3] * math.cos(rad)
        return {x, v[2], z}
    end

    local function rotateZ(v, angle)
        local rad = math.rad(angle)
        local x = v[1] * math.cos(rad) - v[2] * math.sin(rad)
        local y = v[1] * math.sin(rad) + v[2] * math.cos(rad)
        return {x, y, v[3]}
    end

    for i, v in ipairs(vertices) do
        v = rotateX(v, rotationx)
        v = rotateY(v, rotationy)
        v = rotateZ(v, rotationz)

        v[1] = v[1] + x
        v[2] = v[2] + y
        v[3] = v[3] + z

        vertices[i] = v
    end

    lstg.Render4V(
        img,
        vertices[1][1], vertices[1][2], vertices[1][3],
        vertices[2][1], vertices[2][2], vertices[2][3],
        vertices[3][1], vertices[3][2], vertices[3][3],
        vertices[4][1], vertices[4][2], vertices[4][3]
    )
end