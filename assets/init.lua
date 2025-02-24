-- vim: fdm=marker

--[[
-- {{{
local enum State
    "animation"
    "ready"
    "win"
    "gameover"
end

"state_get"
"scores_get"
"quad_width_get"
"field_size_get"

-- }}}
--]]

-- TODO: Делать подсказку наиболее продуктивного хода

local format = string.format
local ceil = math.ceil
local floor = math.floor
local random = math.random
local sin = math.sin
local cos = math.cos

-- XXX: Какого цвета победа?
local function print_win()
    -- {{{

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

    -- }}}
end

local function print_scores()
    -- {{{
    local scores = 0

    print('print_scores')

    local fnt_size = 100
    while true do
        print(1)
        local msg = format("SCORES %d", scores)
        print('msg', msg)
        print(2)
        local msg_w = MeasureText(msg, fnt_size)
        print(3)

        local x = (GetScreenWidth() - msg_w) / 2.
        local y = 0.
        --local x, y = 0, 0
        print('x, y', x, y)
        print(4)

        --DrawText("HER", 0, 0, 100, RED)

        print(
type(msg),
type(x),
type(y),
type(fnt_size),
type(RED)
 )

        DrawText(msg, x, y, fnt_size, GREEN)
        print(5)
        print('print_scores yield')
        scores = coroutine.yield()
    end

    -- }}}
end

local function draw_grid()
    -- {{{

    local quad_width = quad_width_get()
    local field_size = field_size_get()
    --print("draw_grid: quad_width", quad_width, "field_size", field_size)
    --]]

    local anim = {}
    for x = 1, field_size do
        table.insert(anim, {})
        for y = 1, field_size do
            anim[x][y] = {
                random(10, 20), -- коэффициент масштаба
                random()        -- фаза колебания
            }
        end
    end
    --]]
    
    local i = 0
    while true do
        local color = GRAY
        local segments = 20
        local roundness = 0.3

        for x = 0, field_size - 1 do
            for y = 0, field_size - 1 do
                local qw = floor(quad_width)

            local pos_x, pos_y = pos_get()
            local phase = anim[x + 1][y + 1][2]
            local coef = anim[x + 1][y + 1][1]
            local space = coef * (0.8 * math.pi - sin(i + phase))
            --local space = 24. 
            local rect = Rectangle(
                pos_x + x * qw + space,
                pos_y + y * qw + space,
                qw - space * 2,
                qw - space * 2
            )

            local color = RED
            color.a = 70
            DrawRectangleRounded(rect, roundness, segments, color)
            end
        end

        i = i + 0.01
        coroutine.yield()
    end

    -- }}}
end

local function print_gameover()
    -- {{{

    local j = 0
    local fnt_size = 400
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
            if (color[k] >= 100) then
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

    -- }}}
end

-- Вся рисовашка будет на корутинах
local c_print_gameover = coroutine.create(print_gameover)
local c_print_win = coroutine.create(print_win)
local c_print_scores = coroutine.create(print_scores)
local c_draw_grid = coroutine.create(draw_grid)

function update()
    --print('update')
end

function draw_top()
    --print('draw_top')
    local scores = scores_get()
    local state = state_get()
    local ok, err

    --print(format("update: state '%s'", state))

    ok, err = coroutine.resume(c_print_scores, scores)
    if not ok then
        --print("update: err", err)
    end

    if state == 'gameover' then
        coroutine.resume(c_print_gameover)
    elseif state == 'win' then
        coroutine.resume(c_print_gameover)
        ok, msg = coroutine.resume(c_win)

        if ok then
            print('update: ', msg)
        end

    end
end

function draw_bottom()
    --print('draw_bottom')
    coroutine.resume(c_draw_grid)
end

print("init.lua loaded")


print("state_get", state_get())
print("scores_get", scores_get())
print("quad_width_get", quad_width_get())
print("field_size_get", field_size_get())

