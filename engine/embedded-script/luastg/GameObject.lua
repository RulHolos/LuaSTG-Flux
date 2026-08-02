require("bit")

local type = type
local math = require("math")
local math_cos = math.cos
local math_sin = math.sin
local math_rad = math.rad
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
local _CollectGroup = lstg._CollectGroup
local objects = lstg.ObjTable()

local function _UpdateListIterator(checking_world, last_id)
    local id = (last_id == 0) and _UpdateListFirst(checking_world) or _UpdateListNext(last_id, checking_world)
    if id ~= 0 then
        return id, objects[id]
    end
end

local function _DetectListIterator(state, last_id)
    local group = bit.band(state, 15)
    local w = bit.rshift(state, 4)
    local checking_world = (w == 0) and nil or (w - 1)

    local id = (last_id == 0) and _DetectListFirst(group, checking_world) or _DetectListNext(group, last_id, checking_world)
    if id ~= 0 then
        return id, objects[id]
    end
end

---@param group [0, 16]
---@param checking_world integer? optional world flag to check.
function lstg.ObjList(group, checking_world)
    if group < 0 or group >= 16 then
        return _UpdateListIterator, checking_world, 0
    else
        local w = (checking_world == nil) and 0 or (checking_world + 1)
        local state = group + w * 16

        return _DetectListIterator, state, 0
    end
end
---@param group integer
---@param checking_world integer?
---@param dest table pre-allocated table to fill
---@return integer count number of objects written to `dest`
function lstg.Collect_Group(group, checking_world, dest)
    return _CollectGroup(group, checking_world, dest)
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
    local hw = 0.5 * scalex
    local hh = 0.5 * scaley

    local crx = math_cos(math_rad(rotationx))
    local srx = math_sin(math_rad(rotationx))
    local cry = math_cos(math_rad(rotationy))
    local sry = math_sin(math_rad(rotationy))
    local crz = math_cos(math_rad(rotationz))
    local srz = math_sin(math_rad(rotationz))

    local vy1, vz1, vx2, vz2

    vy1 = -hh * crx
    vz1 = -hh * srx
    vx2 = -hw * cry + vz1 * sry
    vz2 = hw * sry + vz1 * cry
    local x1 = vx2 * crz - vy1 * srz + x
    local y1 = vx2 * srz + vy1 * crz + y
    local z1 = vz2 + z

    vx2 = hw * cry + vz1 * sry
    vz2 = -hw * sry + vz1 * cry
    local x2 = vx2 * crz - vy1 * srz + x
    local y2 = vx2 * srz + vy1 * crz + y
    local z2 = vz2 + z

    vy1 = hh * crx
    vz1 = hh * srx
    vx2 = hw * cry + vz1 * sry
    vz2 = -hw * sry + vz1 * cry
    local x3 = vx2 * crz - vy1 * srz + x
    local y3 = vx2 * srz + vy1 * crz + y
    local z3 = vz2 + z

    vx2 = -hw * cry + vz1 * sry
    vz2 = hw * sry + vz1 * cry
    local x4 = vx2 * crz - vy1 * srz + x
    local y4 = vx2 * srz + vy1 * crz + y
    local z4 = vz2 + z

    lstg.Render4V(img, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4)
end