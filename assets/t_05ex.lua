local t =  {
    -- опциональная таблица настроек
    { 
        id = 'options',
        size = 6, 
        description = "WIN",
        tmr_put_time = 0.04,
        tmr_block_time = 1.,
    },

    {
        id = 'initial',
--              1    2    3    4    5    6
--[[ 1 ]]      " ", " ", " ", " ", " ", " ",
--[[ 2 ]]      " ", " ", " ", " ", " ", " ",
--[[ 3 ]]      " ", " ", " ", " ", " ", " ",
--[[ 4 ]]      " ", " ", " ", " ", " ", " ",
--[[ 5 ]]      " ", " ", " ", " ", " ", " ",
--[[ 6 ]]      " ", " ", " ", " ", " ", " ",
    },

    {
        id = 'initial',
--              1    2    3    4    5    6
--[[ 1 ]]      "1024", " ", " ", " ", " ", " ",
--[[ 2 ]]      " ",    " ", " ", " ", " ", " ",
--[[ 3 ]]      " ",    " ", " ", " ", " ", " ",
--[[ 4 ]]      " ",    " ", " ", " ", " ", " ",
--[[ 5 ]]      " ",    " ", " ", " ", " ", " ",
--[[ 6 ]]      " ",    " ", " ", " ", " ", "1024",
    },

}

local moves = {
    { "left", "up" },
    { "right", "up" },
    { "left", "down" },
    { "left", "up" },
}

math.randomseed(os.time())

local n = math.random(1, #moves)
print("n", n)
local couple = moves[n]

for _, move in ipairs(couple) do
    table.insert(t, {
        id = "input", move
    })
end

table.insert(t, {
    id = "input", "right"
})

return t
