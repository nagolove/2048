return {
    -- опциональная таблица настроек
    { 
        id = 'options',
        size = 6, 
        description = "ДОБАВИТЬ ВЫСЫПАНИЕ",
    },

    {
        id = 'initial',
        " ", "8", "4", "8", " ", " ",
        " ", " ", " ", " ", " ", " ",
        " ", "4", "2", "8", " ", " ",
        " ", "4", "2", "8", " ", " ",
        " ", "8", "4", "8", " ", " ",
        " ", "8", "4", "8", " ", " ",
    },

    { 
        id = 'input',
        "down",
    },

    {
        id = 'assert',
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  "8",  "4",  " ",  " ",  " ", 
        " ", "16",  "8", "32",  " ",  " ", 
        " ",  "8",  "4",  "8",  " ",  " ", 
    },

    {
        id = 'initial',
        " ",  "2",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
        " ",  " ",  " ",  " ",  " ",  " ", 
    },

    {
        id = 'input',
        'left',
    },

    {
        id = 'assert',
         "2",  " ",  " ",  " ",  " ", " ", 
         " ",  " ",  " ",  " ",  " ", " ", 
         " ",  " ",  " ",  " ",  " ", " ", 
         "8",  "4",  " ",  " ",  " ", " ", 
         "16", "8",  "32", " ",  " ", " ", 
         "8",  "4",  "8",  " ",  " ", " ", 
    },

    {
        id = 'input',
        -- Допустимо несколько значений
        'right',
    },

    {
        id = 'assert',
          " ",  " ", " ", " ",  " ",  "2",
          " ",  " ", " ", " ",  " ",  " ",
          " ",  " ", " ", " ",  " ",  " ",
          " ",  " ", " ", " ",  "8",  "4",
          " ",  " ", " ", "16", "8",  "32",
          " ",  " ", " ", "8",  "4",  "8",
    },

}
--]]
