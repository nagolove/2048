--[[
local enum State
    "animation"
    "ready"
    "win"
    "gameover"
end

state_get()

--]]

local format = string.format
local ceil = math.ceil
local floor = math.floor
local random = math.random

-- XXX: Какого цвета победа?
local function win()

    local fnt_size = 300
    local color = { 0, 0, 0 }
    local x, y = 0, 0
    local colord = 1
    local msg = "WIN"
    local msg_w = MeasureText(msg, fnt_size)
    local max_y, max_x = GetScreenWidth() - msg_w, GetScreenWidth() - msg_w
    local c = RED

    while true do
        DrawText(msg, x, j, fnt_size, c)
        
        color[1] = color[1] + colord
        color[2] = color[2] + colord
        color[3] = color[3] + colord

        for k = 1, 3 do
            if (color[k] >= 255) then
                colord = -colord
            end
        end

        y = y + 10

        if y > max_y then
            y = -fnt_size
        end

        x = x + random(10)

        if x > max_x then
            x = 0
        end

        print('win yield')
        coroutine.yield()
    end

end

local function gameover()

    local j = 0
    local fnt_size = 300
    local color = { 0, 0, 0 }
    local x = 0
    local colord = 1
    while true do
        local c = Color(ceil(color[1]), ceil(color[2]), ceil(color[3]))

        --local c = RED
        DrawText("GAVEOVER", x, j, fnt_size, c)
        --DrawText("HELLO YEPdev", 100, j, fnt_size, RED)

        --[[
        color[1] = color[1] + random() * 0.5
        color[2] = color[2] + random() * 0.5
        color[3] = color[3] + random() * 0.5
        --]]
        
        color[1] = color[1] + colord
        color[2] = color[2] + colord
        color[3] = color[3] + colord


        for k = 1, 3 do
            if (color[k] >= 255) then
                colord = -colord
            end
        end

        j = j + 10

        if j > GetScreenHeight() then
            j = -fnt_size
        end

        x = x + random(10)

        if x > GetScreenWidth() then
            x = 0
        end

        print('gameover yield')
        coroutine.yield()
    end

end

local c_gameover = coroutine.create(gameover)
local c_win = coroutine.create(win)

function update()
    local state = state_get()
    print(format('update: state '%s'', state))
    if state == 'gameover' then
        coroutine.resume(c_gameover)
    elseif state == 'win' then
        local ok, msg = coroutine.resume(c_win)
        if ok then
            print('update: ', msg)
        end
    end
end

print("init.lua loaded")
